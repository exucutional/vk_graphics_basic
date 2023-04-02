#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

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

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  noiseMap = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = m_noiseSize * m_noiseSize * sizeof(float),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name        = "noise_map",
  });

  gbuffer = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = vk::Extent3D{ m_width, m_height, 1 },
    .name       = "gbuffer",
    .format     = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
  });

  fogMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent     = vk::Extent3D{ m_width / 4, m_height / 4, 1 },
    .name       = "fog_map",
    .format     = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment
  });

  fogDepthMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width / 4, m_height / 4, 1},
    .name = "fog_depth_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  quadVertexBuffer = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = 9 * sizeof(QuadVertexInput),
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name        = "quad_vertex_buffer",
  });

  quadIndexBuffer = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = 16 * sizeof(uint),
    .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name        = "quad_index_buffer"
  });

  m_uboMappedMem = constants.map();

  const std::vector<QuadVertexInput> vertices =
  {
    { { -1.27f, -1.27f, 1.27f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } }, // 0
    { { -1.27f, -1.27f, -1.27f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f } },// 1
    { { 1.27f, -1.27f, -1.27f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } }, // 2
    { { 1.27f, -1.27f, 1.27f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 3
    { { -1.27f, -1.27f, 0.0f, 0.0f }, { 1.0f, 0.5f, 0.0f, 0.0f } },  // 4
    { { 0.0f, -1.27f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.0f, 0.0f } },    // 5
    { { 0.0f, -1.27f, 1.27f, 0.0f }, { 0.5f, 0.0f, 0.0f, 0.0f } },   // 6
    { { 1.27f, -1.27f, 0.0f, 0.0f }, { 0.0f, 0.5f, 0.0f, 0.0f } },   // 7
    { { 0.0f, -1.27f, -1.27f, 0.0f }, { 0.5f, 1.0f, 0.0f, 0.0f } },  // 8
    // 0 - 6 - 3
    // |   |   |
    // 4 - 5 - 7
    // |   |   |
    // 1 - 8 - 2
  };
  const std::vector<uint> indices =
  {
    0, 4, 6, 5, 6, 5, 3, 7, 7, 5, 2, 8, 8, 5, 1, 4
  };
  void* quadVertexMappedMem = quadVertexBuffer.map();
  memcpy(quadVertexMappedMem, vertices.data(), vertices.size() * sizeof(QuadVertexInput));
  quadVertexBuffer.unmap();
  void* quadIndexMappedMem = quadIndexBuffer.map();
  memcpy(quadIndexMappedMem, indices.data(), indices.size() * sizeof(uint));
  quadIndexBuffer.unmap();
  m_noiseMappedMem = noiseMap.map();
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
  fogMap.reset();
  fogDepthMap.reset();
  gbuffer.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
  quadVertexBuffer = etna::Buffer();
  quadIndexBuffer = etna::Buffer();
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
  {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.tesc.spv", 
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.tese.spv"
  });
  etna::create_program("simple_shadow",
  {
     VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv",
     VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.tesc.spv",
     VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.tese.spv"
  });
  etna::create_program("simple_fog",
  {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/fog.frag.spv",
  });
  etna::create_program("simple_deferred",
  {
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/deferred.frag.spv",
  });
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

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .inputAssemblyConfig =
        {
          .topology = vk::PrimitiveTopology::ePatchList
        },
      .tessellationConfig = 
        {
          .patchControlPoints = 4
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR8G8B8A8Srgb},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .inputAssemblyConfig =
        {
          .topology = vk::PrimitiveTopology::ePatchList
        },
      .tessellationConfig = 
        {
          .patchControlPoints = 4
        },
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_fogPipeline = pipelineManager.createGraphicsPipeline("simple_fog",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR8G8B8A8Srgb}
        }
    });
  m_deferredPipeline = pipelineManager.createGraphicsPipeline("simple_deferred",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())}
        }
    });
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
    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::DrawTerrainCmd(VkCommandBuffer a_cmdBuff, const float4x4 &a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = quadVertexBuffer.get();
  VkBuffer indexBuf  = quadIndexBuffer.get();

  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  pushConst2M.noiseSize  = m_noiseSize;
  vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
    stageFlags, 0, sizeof(pushConst2M), &pushConst2M);
  vkCmdDrawIndexed(a_cmdBuff, 16, 1, 0, 0, 0);
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
    auto simpleShadowInfo = etna::get_shader_program("simple_shadow");

    auto set = etna::create_descriptor_set(simpleShadowInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, noiseMap.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();
    etna::RenderTargetState renderTargets(a_cmdBuff, {2048, 2048}, {}, shadowMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_shadowPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawTerrainCmd(a_cmdBuff, m_lightMatrix);
  }

  //// draw scene to gbuffer
  //
  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_material");

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, noiseMap.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{gbuffer.get(), gbuffer.getView({})}}, mainViewDepth);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawTerrainCmd(a_cmdBuff, m_worldViewProj);
  }

  //// draw scene to fogMap
  //
  {
    auto simpleShadowInfo = etna::get_shader_program("simple_shadow");

    auto set = etna::create_descriptor_set(simpleShadowInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, noiseMap.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();
    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width/4, m_height/4}, {}, fogDepthMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_shadowPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    DrawTerrainCmd(a_cmdBuff, m_worldViewProj);
  }


  //// draw fog to fogMap
  //
  {
    auto fogInfo = etna::get_shader_program("simple_fog");

    auto set = etna::create_descriptor_set(fogInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, fogDepthMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

    VkDescriptorSet vkSet = set.getVkSet();
    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width/4, m_height/4}, {{fogMap.get(), fogMap.getView({})}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fogPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_fogPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  }

  //// draw final scene with fog to screen
  //
  {
    auto deferredInfo = etna::get_shader_program("simple_deferred");

    auto set = etna::create_descriptor_set(deferredInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, gbuffer.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, fogMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferredPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_deferredPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
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
