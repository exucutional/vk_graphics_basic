#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>
#include <random>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  positionMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "position_map",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  normalMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "normal_map",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  albedoMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "albedo_map",
    .format = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  rawSSAO = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "raw_ssao",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  blurredSSAO = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "blurred_ssao",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  ssaoSamples = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float4) * m_ssaoSampleSize,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "ssao_samples"
  });

  ssaoNoise = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float4) * m_ssaoNoiseSize * m_ssaoNoiseSize,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "ssao_noise"
  });

  gaussianCoeffs = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float) * (m_gauss_window / 4 + 1) * 4,
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU
  });

  m_uboMappedMem = constants.map();

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0, 1.0);

  void *ssaoSamplesMappedMem = ssaoSamples.map();
  std::vector<float4> ssaoSamplesVec;
  ssaoSamplesVec.reserve(m_ssaoSampleSize);
  for (int i = 0; i < m_ssaoSampleSize; ++i)
  {
    float4 sample = { dis(gen) * 2.f - 1.f, dis(gen) * 2.f - 1.f, dis(gen), 0.0 };
    float4 nSample = LiteMath::normalize(sample) * dis(gen);
    ssaoSamplesVec.push_back(nSample);
  }
  memcpy(ssaoSamplesMappedMem, ssaoSamplesVec.data(), ssaoSamplesVec.size() * sizeof(float4));
  ssaoSamples.unmap();

  void *ssaoNoiseMappedMem = ssaoNoise.map();
  std::vector<float4> ssaoNoiseVec;
  ssaoNoiseVec.reserve(m_ssaoNoiseSize * m_ssaoNoiseSize);
  for (int i = 0; i < m_ssaoNoiseSize * m_ssaoNoiseSize; ++i)
  {
    float4 noise = { dis(gen) * 2.f - 1.f, dis(gen) * 2.f - 1.f, 0.f, 0.0f };
    ssaoNoiseVec.push_back(noise);
  }
  memcpy(ssaoNoiseMappedMem, ssaoNoiseVec.data(), ssaoNoiseVec.size() * sizeof(float4));
  ssaoNoise.unmap();

  void *gaussianCoeffsMappedMem = gaussianCoeffs.map();
  std::vector<float> coeffs((m_gauss_window / 4 + 1) * 4, 0.0f);
  float halfwindow = (m_gauss_window - 1) / 2;
  float sigma      = halfwindow / 3;// 68–95–99.7 rule
  float sigma2     = 2 * sigma * sigma;
  for (int i = 0; i < m_gauss_window; i++)
  {
    float val = -(i - halfwindow) * (i - halfwindow) / sigma2;
    coeffs[i] = exp(val);
  }
  memcpy(gaussianCoeffsMappedMem, coeffs.data(), sizeof(float) * coeffs.size());
  gaussianCoeffs.unmap();
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  positionMap.reset();
  normalMap.reset();
  albedoMap.reset();
  rawSSAO.reset();
  blurredSSAO.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
  ssaoSamples = etna::Buffer();
  ssaoNoise = etna::Buffer();
  gaussianCoeffs = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, 512, 512);
  m_pFSQuad->Create(m_context->getDevice(),
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad.frag.spv",
    vk_utils::RenderTargetInfo2D{
      .size          = VkExtent2D{ m_width, m_height },// this is debug full screen quad
      .format        = m_swapchain.GetFormat(),
      .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,// seems we need LOAD_OP_LOAD if we want to draw quad to part of screen
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    }
  );
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_deferred",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/deferred.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_final_deferred",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow_deferred.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"});
  etna::create_program("simple_ssao",
    { VK_GRAPHICS_BASIC_ROOT"/resources/shaders/ssao.frag.spv", VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv" });
  etna::create_program("simple_gblur",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/gblur.comp.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_context->getDevice(), dtypes, 2);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.getView({}), defaultSampler.get(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto blendAttachment = vk::PipelineColorBlendAttachmentState
    {
      .blendEnable = false,
      .colorWriteMask = vk::ColorComponentFlagBits::eR
        | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB
        | vk::ColorComponentFlagBits::eA
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_deferredPipeline = pipelineManager.createGraphicsPipeline("simple_deferred",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .blendingConfig =
        {
          .attachments = {blendAttachment, blendAttachment, blendAttachment}
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat, vk::Format::eR8G8B8A8Srgb},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_finalDeferredPipeline = pipelineManager.createGraphicsPipeline("simple_final_deferred",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = { static_cast<vk::Format>(m_swapchain.GetFormat()) },
        }
    });
  m_ssaoPipeline = pipelineManager.createGraphicsPipeline("simple_ssao",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32Sfloat},
        }
    });
  m_ssaoBlurPipeline = pipelineManager.createComputePipeline("simple_gblur", {});
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    auto inst         = m_pScnMgr->GetInstanceInfo(i);
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
    pushConst2M.albedoId = i;
    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {2048, 2048}, {}, shadowMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    DrawSceneCmd(a_cmdBuff, m_lightMatrix);
  }

  {
    //// draw scene to gbuffers
    //
    {
      auto simpleDeferredInfo = etna::get_shader_program("simple_deferred");

      auto set = etna::create_descriptor_set(simpleDeferredInfo.getDescriptorLayoutId(0), a_cmdBuff,
      {
        etna::Binding {0, constants.genBinding()}
      });

      VkDescriptorSet vkSet = set.getVkSet();

      etna::RenderTargetState renderTargets(a_cmdBuff,
        { m_width, m_height },
        {
          { positionMap.get(), positionMap.getView({}) },
          { normalMap.get(), normalMap.getView({}) },
          { albedoMap.get(), albedoMap.getView({}) }
        },
        mainViewDepth);

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferredPipeline.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_deferredPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

      DrawSceneCmd(a_cmdBuff, m_worldViewProj);
    }
    //// calc ssao
    //
    {
      auto simpleSSAOInfo = etna::get_shader_program("simple_ssao");

      auto set = etna::create_descriptor_set(simpleSSAOInfo.getDescriptorLayoutId(0), a_cmdBuff,
      {
        etna::Binding {0, constants.genBinding()},
        etna::Binding {1, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding {2, normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding {3, ssaoSamples.genBinding()},
        etna::Binding {4, ssaoNoise.genBinding()}
      });

      VkDescriptorSet vkSet = set.getVkSet();

      etna::RenderTargetState renderTargets(a_cmdBuff, { m_width, m_height }, { { rawSSAO.get(), rawSSAO.getView({}) } }, {});

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_ssaoPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

      vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    }

    //// blur ssao
    //
    if (m_ssao && m_blur_ssao)
    {
      auto simpleBlurInfo = etna::get_shader_program("simple_gblur");
      auto set = etna::create_descriptor_set(simpleBlurInfo.getDescriptorLayoutId(0), a_cmdBuff,
      {
        etna::Binding {0, rawSSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
        etna::Binding {1, blurredSSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
        etna::Binding {2, gaussianCoeffs.genBinding()},
        etna::Binding {3, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
      });
      VkDescriptorSet vkSet = set.getVkSet();
      etna::flush_barriers(a_cmdBuff);

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_ssaoBlurPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_ssaoBlurPipeline.getVkPipeline());
      vkCmdDispatch(a_cmdBuff, m_width / 32 + 1, m_height / 32 + 1, 1);
    }

    //// apply shading and ssao
    //
    {

      auto simpleFinalDeferredInfo = etna::get_shader_program("simple_final_deferred");

      etna::DescriptorSet set;
      if (m_ssao && m_blur_ssao)
        set = etna::create_descriptor_set(simpleFinalDeferredInfo.getDescriptorLayoutId(0), a_cmdBuff,
        {
          etna::Binding {0, constants.genBinding()},
          etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {2, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {3, normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {4, albedoMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {5, blurredSSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        });
      else
        set = etna::create_descriptor_set(simpleFinalDeferredInfo.getDescriptorLayoutId(0), a_cmdBuff,
        {
          etna::Binding {0, constants.genBinding()},
          etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {2, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {3, normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {4, albedoMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding {5, rawSSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        });
      VkDescriptorSet vkSet = set.getVkSet();

      etna::RenderTargetState renderTargets(a_cmdBuff, { m_width, m_height }, { { a_targetImage, a_targetImageView } }, {});

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_finalDeferredPipeline.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_finalDeferredPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

      vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    }
  }

  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
