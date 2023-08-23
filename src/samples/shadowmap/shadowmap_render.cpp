#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>
#include <random>
#include <algorithm>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>
#include "loader_utils/ply_reader.hpp"

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

  //void* particlesMappedMem = particles.map();
  //std::vector<Particle> particlesVec(m_particlesMaxCount);
  //std::random_device rd;
  //std::mt19937 gen(rd());
  //std::uniform_real_distribution<> disn(-1.0f, 1.0f);
  //std::uniform_real_distribution<> disp(0.0f, 1.0f);
  //for (auto& particle : particlesVec)
  //{
  //  particle.color    = float4(disp(gen), disp(gen), disp(gen), 1.0f);
  //  particle.position = float2(0.0f);
  //  particle.velocity = LiteMath::normalize(float2(disn(gen), disn(gen)));
  //  particle.time     = 0.0f;
  //}

  //memcpy(particlesMappedMem, particlesVec.data(), particlesVec.size() * sizeof(Particle));
  //particles.unmap();
  //m_particlesSpawnCountMappedMem = particlesSpawnCount.map();
  //*static_cast<uint*>(m_particlesSpawnCountMappedMem) = 0;
}

void SimpleShadowmapRender::LoadPointScene(int lod_n, const char *path)
{
  std::vector<ply::Vertex> pointCloud;
  std::vector<ply::Face> faces;
  ply::read_ply_file(path, pointCloud, faces);
  pointLods[lod_n] = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float3) * pointCloud.size(),
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferSrc,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name        = std::format("bunny_lod_{}", lod_n)
  });
  pointLodCount[lod_n] = pointCloud.size();
  auto buffer = pointLods[lod_n].map();
  for (auto &v : pointCloud)
  {
    float3 pos{ v.x, v.y, v.z };
    memcpy(buffer, &pos, sizeof(float3));
    buffer += sizeof(float3);
  }
  pointLods[lod_n].unmap();
}

void SimpleShadowmapRender::LoadPolyScene(int lod_n, const char *path)
{
  std::vector<ply::Vertex> pointCloud;
  std::vector<ply::Face> faces;
  ply::read_ply_file(path, pointCloud, faces);
  vertexLods[lod_n] = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float3) * pointCloud.size(),
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferSrc,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name        = std::format("bunny_vertex_lod_{}", lod_n)
  });
  indexLods[lod_n] = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(uint3) * faces.size(),
    .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferSrc,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name        = std::format("bunny_index_lod_{}", lod_n)
  });
  vertexLodCount[lod_n] = pointCloud.size();
  indexLodCount[lod_n] = faces.size();
  auto vertexBuffer = vertexLods[lod_n].map();
  for (auto &v : pointCloud)
  {
    float3 pos{ v.x, v.y, v.z };
    memcpy(vertexBuffer, &pos, sizeof(float3));
    vertexBuffer += sizeof(float3);
  }
  vertexLods[lod_n].unmap();
  auto indexBuffer = indexLods[lod_n].map();
  for (auto& f : faces)
  {
    memcpy(indexBuffer, f.verts, sizeof(int)*f.nverts);
    indexBuffer += sizeof(int3);
  }
  indexLods[lod_n].unmap();
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();
  vertexLods.resize(6);
  indexLods.resize(6);
  vertexLodCount.resize(6);
  indexLodCount.resize(6);
  pointLods.resize(6);
  pointLodCount.resize(6);
  for (int i = 0; i < 6; ++i)
  {
    LoadPolyScene(i, std::format(VK_GRAPHICS_BASIC_ROOT"/resources/scenes/bunny/reconstruction/bun_lod_{}.ply", i).c_str());
    LoadPointScene(i, std::format(VK_GRAPHICS_BASIC_ROOT"/resources/scenes/bunny/data/bun_lod_{}.ply", i).c_str());
  }
  const auto maxVertexCount = *std::max_element(vertexLodCount.cbegin(), vertexLodCount.cend());
  const auto maxIndexCount  = *std::max_element(indexLodCount.cbegin(), indexLodCount.cend());
  const auto maxPointCount  = *std::max_element(pointLodCount.cbegin(), pointLodCount.cend());
  vertexBuf = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float3) * maxVertexCount,
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name        = "bunny_vertex",
  });
  indexBuf = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(uint3) * maxIndexCount,
    .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name        = "bunny_index",
  });
  pointBuf = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float3) * maxPointCount,
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name        = "bunny_points",
  });
  vertexBufferCopy = true;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  pointBuf = etna::Buffer();
  vertexBuf = etna::Buffer();
  indexBuf = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_points",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/point.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/point.vert.spv"});
  etna::create_program("simple_polygons",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/point.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/point.vert.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  //std::vector<etna::VertexShaderInputDescription::Binding> bindings;
  ////bindings.push_back({});
  //etna::VertexShaderInputDescription pointInput =
  //  {
  //    .bindings = bindings;
  //  };

  etna::VertexByteStreamFormatDescription::Attribute attributePosition =
  {
    .format = vk::Format::eR32G32B32Sfloat,
    .offset = 0,
  };

  etna::VertexShaderInputDescription::Binding binding =
  {
    .byteStreamDescription =
      {
        .stride = sizeof(float3),
        .attributes = { attributePosition }
      }
  };

  etna::VertexShaderInputDescription positionInput =
  {
    .bindings = { binding },
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicPointPipeline = pipelineManager.createGraphicsPipeline("simple_points",
    {
      .vertexShaderInput = positionInput,
      .inputAssemblyConfig = {
        .topology = vk::PrimitiveTopology::ePointList,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_basicPolygonPipeline = pipelineManager.createGraphicsPipeline("simple_polygons",
    {
      .vertexShaderInput = positionInput,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}

void SimpleShadowmapRender::DrawPointSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkBuffer buf             = pointBuf.get();
  VkDeviceSize zero_offset = 0u;
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &buf, &zero_offset);

  pushConst2M.model     = float4x4();
  pushConst2M.pointSize = m_pointSize;
  pushConst2M.projView  = a_wvp;
  pushConst2M.instanceCountSqrt = (int)sqrt(m_instanceCount);

  vkCmdPushConstants(a_cmdBuff, m_basicPointPipeline.getVkPipelineLayout(), stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

  vkCmdDraw(a_cmdBuff, pointLodCount[m_lod], m_instanceCount, 0, 0);
  m_pointCount   = m_instanceCount * pointLodCount[m_lod];
  m_polygonCount = 0;
}

void SimpleShadowmapRender::DrawPolySceneCmd(VkCommandBuffer a_cmdBuff, const float4x4 &a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkBuffer vbuf = vertexBuf.get();
  VkBuffer ibuf = indexBuf.get();
  VkDeviceSize zero_offset = 0u;
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vbuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, ibuf, zero_offset, VK_INDEX_TYPE_UINT32);

  pushConst2M.model             = float4x4();
  pushConst2M.pointSize         = m_pointSize;
  pushConst2M.projView          = a_wvp;
  pushConst2M.instanceCountSqrt = (int)sqrt(m_instanceCount);

  vkCmdPushConstants(a_cmdBuff, m_basicPointPipeline.getVkPipelineLayout(), stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

  vkCmdDrawIndexed(a_cmdBuff, indexLodCount[m_lod], m_instanceCount, 0, 0, 0);
  m_polygonCount = m_instanceCount * vertexLodCount[m_lod];
  m_pointCount = 0;
}

/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));
  static auto last_lod = m_lod;
  if (vertexBufferCopy || last_lod != m_lod)
  {
    VkBufferCopy copyVertexRegion {
      .srcOffset = 0,
      .dstOffset = 0,
      .size      = vertexLodCount[m_lod] * sizeof(float3)
    };
    VkBufferCopy copyIndexRegion {
      .srcOffset = 0,
      .dstOffset = 0,
      .size      = indexLodCount[m_lod] * sizeof(uint3)
    };
    VkBufferCopy copyPointRegion {
      .srcOffset = 0,
      .dstOffset = 0,
      .size      = pointLodCount[m_lod] * sizeof(float3)
    };
    vkCmdCopyBuffer(a_cmdBuff, vertexLods[m_lod].get(), vertexBuf.get(), 1, &copyVertexRegion);
    vkCmdCopyBuffer(a_cmdBuff, indexLods[m_lod].get(), indexBuf.get(), 1, &copyIndexRegion);
    vkCmdCopyBuffer(a_cmdBuff, pointLods[m_lod].get(), pointBuf.get(), 1, &copyPointRegion);
    vertexBufferCopy = false;
    last_lod = m_lod;
  }

  //// draw final scene to screen
  //
  if (m_pointRendering)
  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_points");

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, mainViewDepth);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicPointPipeline.getVkPipeline());

    DrawPointSceneCmd(a_cmdBuff, m_worldViewProj);
  }
  else
  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_polygons");

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, mainViewDepth);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicPolygonPipeline.getVkPipeline());

    DrawPolySceneCmd(a_cmdBuff, m_worldViewProj);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
