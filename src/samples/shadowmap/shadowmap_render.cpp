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

  particles = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(Particle) * m_particlesMaxCount,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "particles"
  });

  particlesSpawnCount = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(uint),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "particlesSpawnCount"
  });

  void* particlesMappedMem = particles.map();
  std::vector<Particle> particlesVec(m_particlesMaxCount);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> disn(-1.0f, 1.0f);
  std::uniform_real_distribution<> disp(0.0f, 1.0f);
  for (auto& particle : particlesVec)
  {
    particle.color    = float4(disp(gen), disp(gen), disp(gen), 1.0f);
    particle.position = float2(0.0f);
    particle.velocity = LiteMath::normalize(float2(disn(gen), disn(gen)));
    particle.time     = 0.0f;
  }

  memcpy(particlesMappedMem, particlesVec.data(), particlesVec.size() * sizeof(Particle));
  particles.unmap();
  m_particlesSpawnCountMappedMem = particlesSpawnCount.map();
  *static_cast<uint*>(m_particlesSpawnCountMappedMem) = 0;
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  particles = etna::Buffer();
  particlesSpawnCount = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_particles",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_particles.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_particles.vert.spv"});
  etna::create_program("emitter", { VK_GRAPHICS_BASIC_ROOT "/resources/shaders/emitter.comp.spv" });
  etna::create_program("compute", { VK_GRAPHICS_BASIC_ROOT "/resources/shaders/compute.comp.spv" });
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  //std::vector<etna::VertexShaderInputDescription::Binding> bindings;
  ////bindings.push_back({});
  //etna::VertexShaderInputDescription pointInput =
  //  {
  //    .bindings = bindings;
  //  };

  etna::VertexByteStreamFormatDescription::Attribute attributeColor =
  {
    .format = vk::Format::eR32G32B32A32Sfloat,
    .offset = 0,
  };

  etna::VertexByteStreamFormatDescription::Attribute attributePosition =
  {
    .format = vk::Format::eR32G32Sfloat,
    .offset = sizeof(float4),
  };

  etna::VertexByteStreamFormatDescription::Attribute attributeVelocity =
  {
    .format = vk::Format::eR32G32Sfloat,
    .offset = sizeof(float4) + sizeof(float2),
  };

  etna::VertexByteStreamFormatDescription::Attribute attributeTime =
  {
    .format = vk::Format::eR32Sfloat,
    .offset = sizeof(float4) + sizeof(float2) + sizeof(float2),
  };

  etna::VertexShaderInputDescription::Binding binding =
  {
    .byteStreamDescription =
      {
        .stride = sizeof(Particle),
        .attributes = { attributeColor, attributePosition, attributeVelocity, attributeTime}
      }
  };

  etna::VertexShaderInputDescription particleInput =
  {
    .bindings = { binding },
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_particles",
    {
      .vertexShaderInput = particleInput,
      .inputAssemblyConfig = {
        .topology = vk::PrimitiveTopology::ePointList,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });

  m_emitterPipeline = pipelineManager.createComputePipeline("emitter", {});
  m_computePipeline = pipelineManager.createComputePipeline("compute", {});
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  //// Emit particles
  //
  {
    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_COMPUTE_BIT);

    pushConst2Emitter.particlesLifetime = m_particlesLifetime;
    pushConst2Emitter.particlesMaxCount = m_particlesMaxCount;
    pushConst2Emitter.particlesSpawnMaxCount = m_particlesSpawnMaxCount;
    pushConst2Emitter.particlesVelocityScale = m_particlesVelocityScale;

    auto emitterInfo = etna::get_shader_program("emitter");

    auto set = etna::create_descriptor_set(emitterInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, particles.genBinding()},
      etna::Binding {1, particlesSpawnCount.genBinding()},
    });

    VkDescriptorSet vkSet = set.getVkSet();

    vkCmdPushConstants(a_cmdBuff, m_emitterPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2Emitter), &pushConst2Emitter);

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_emitterPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    *static_cast<uint *>(m_particlesSpawnCountMappedMem) = 0;

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_emitterPipeline.getVkPipeline());

    vkCmdDispatch(a_cmdBuff, m_particlesMaxCount / 32 + 1, 1, 1);
  }

  //// Update particles
  //
  {
    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_COMPUTE_BIT);

    pushConst2Compute.particlesMaxCount = m_particlesMaxCount;
    pushConst2Compute.M = m_M;

    auto computeInfo = etna::get_shader_program("compute");

    auto set = etna::create_descriptor_set(computeInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, particles.genBinding()},
    });

    VkDescriptorSet vkSet = set.getVkSet();

    vkCmdPushConstants(a_cmdBuff, m_computePipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2Compute), &pushConst2Compute);

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_computePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline.getVkPipeline());

    vkCmdDispatch(a_cmdBuff, m_particlesMaxCount / 32 + 1, 1, 1);
  }

  //// draw final scene to screen
  //
  {
    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

    auto simpleMaterialInfo = etna::get_shader_program("simple_particles");

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, mainViewDepth);

    VkBuffer buf = particles.get();
    VkDeviceSize zero_offset = 0u;
    vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &buf, &zero_offset);

    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(float), &m_pointSize);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());

    vkCmdDraw(a_cmdBuff, m_particlesMaxCount, 1, 0, 0);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
