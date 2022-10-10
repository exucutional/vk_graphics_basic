#include "simple_compute.h"

#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <vk_utils.h>
#include <random>
#include <numeric>
#include <chrono>

SimpleCompute::SimpleCompute(uint32_t a_length) : m_length(a_length)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif
}

void SimpleCompute::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleCompute::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  m_instanceExtensions.clear();
  for (uint32_t i = 0; i < a_instanceExtensionsCount; ++i) {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }
  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.compute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBufferCompute = vk_utils::createCommandBuffers(m_device, m_commandPool, 1)[0];
  
  m_pCopyHelper = std::make_shared<vk_utils::SimpleCopyHelper>(m_physicalDevice, m_device, m_transferQueue, m_queueFamilyIDXs.compute, 8*1024*1024);
}


void SimpleCompute::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pApplicationName = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "SimpleCompute";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);
  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleCompute::CreateDevice(uint32_t a_deviceId)
{
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.compute, 0, &m_computeQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}


void SimpleCompute::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             2}
  };

  // Создание и аллокация буферов
  m_A = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_res = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  vk_utils::allocateAndBindWithPadding(m_device, m_physicalDevice, {m_A, m_res}, 0);

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 1);

  // Создание descriptor set для передачи буферов в шейдер
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_A);
  m_pBindings->BindBuffer(1, m_res);
  m_pBindings->BindEnd(&m_sumDS, &m_sumDSLayout);

  // Заполнение буферов
  std::vector<float> values(m_length);
  std::random_device rd;
  std::mt19937 gen(0xdeadbeef);
  std::uniform_real_distribution<> dis(-10000.0f, 10000.0f);
  for (uint32_t i = 0; i < values.size(); ++i)
    values[i] = (float)dis(gen);

  m_pCopyHelper->UpdateBuffer(m_A, 0, values.data(), sizeof(float) * values.size());
}

void SimpleCompute::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkPipeline pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  // Заполняем буфер команд
  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &m_sumDS, 0, NULL);

  vkCmdPushConstants(a_cmdBuff, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_length), &m_length);

  vkCmdDispatch(a_cmdBuff, m_length / 32 + 1, 1, 1);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}


void SimpleCompute::CleanupPipeline()
{
  if (m_cmdBufferCompute)
  {
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_cmdBufferCompute);
  }

  vkDestroyBuffer(m_device, m_A, nullptr);
  vkDestroyBuffer(m_device, m_res, nullptr);

  vkDestroyPipelineLayout(m_device, m_layout, nullptr);
  vkDestroyPipeline(m_device, m_pipelines[0], nullptr);
  vkDestroyPipeline(m_device, m_pipelines[1], nullptr);
}


void SimpleCompute::Cleanup()
{
  CleanupPipeline();

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  }
}


void SimpleCompute::CreateComputePipeline()
{
  VkShaderModule shaderModules[2] = {};
  VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2] = {};
  int shader_i = 0;
  for (auto shader : { "../resources/shaders/simple.comp.spv",
    "../resources/shaders/simple_shared.comp.spv" })
  {
    // Загружаем шейдер
    std::vector<uint32_t> code          = vk_utils::readSPVFile(shader);
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode                    = code.data();
    createInfo.codeSize                 = code.size() * sizeof(uint32_t);

    // Создаём шейдер в вулкане
    VK_CHECK_RESULT(vkCreateShaderModule(m_device, &createInfo, NULL, &shaderModules[shader_i]));

    shaderStageCreateInfos[shader_i].sType   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfos[shader_i].stage   = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfos[shader_i].module  = shaderModules[shader_i];
    shaderStageCreateInfos[shader_i++].pName = "main";
  }

  VkPushConstantRange pcRange = {};
  pcRange.offset = 0;
  pcRange.size = sizeof(m_length);
  pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // Создаём layout для pipeline
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts    = &m_sumDSLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, NULL, &m_layout));

  VkComputePipelineCreateInfo pipelineCreateInfos[2] = {};
  pipelineCreateInfos[0].sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineCreateInfos[0].stage  = shaderStageCreateInfos[0];
  pipelineCreateInfos[0].layout = m_layout;
  pipelineCreateInfos[1].sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineCreateInfos[1].stage  = shaderStageCreateInfos[1];
  pipelineCreateInfos[1].layout = m_layout;

  // Создаём pipeline - объект, который выставляет шейдер и его параметры
  VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 2, pipelineCreateInfos, NULL, m_pipelines));

  vkDestroyShaderModule(m_device, shaderModules[0], nullptr);
  vkDestroyShaderModule(m_device, shaderModules[1], nullptr);
}


void SimpleCompute::Execute()
{
  SetupSimplePipeline();
  CreateComputePipeline();

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_cmdBufferCompute;

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, NULL, &m_fence));
  const char *vulkan_s[2] = { "Vulkan single run", "Vulkan shared memory single run" };
  for (int i = 0; i < 2; i++)
  {
    std::cout << vulkan_s[i] << std::endl;
    BuildCommandBufferSimple(m_cmdBufferCompute, m_pipelines[i]);
    auto start = std::chrono::high_resolution_clock::now();
    // Отправляем буфер команд на выполнение
    VK_CHECK_RESULT(vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_fence));
    //Ждём конца выполнения команд
    VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 100000000000));
    auto end_vulkan = std::chrono::steady_clock::now();
    std::vector<float> values(m_length);
    m_pCopyHelper->ReadBuffer(m_res, 0, values.data(), sizeof(float) * values.size());
    std::cout << "-> Result:                                    "
      << std::accumulate(std::begin(values), std::end(values), 0.0) / values.size() << std::endl;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed_vulkan = end_vulkan - start;
    std::chrono::duration<float> elapsed_total  = end - start;
    std::cout << "-> Compute time:                              " << elapsed_vulkan.count() << "s\n";
    std::cout << "-> Compute + Reading buffer + Calc mean time: " << elapsed_total.count() << "s\n";
    vkResetFences(m_device, 1, &m_fence);
  }
  std::chrono::duration<float> time_sum[2] = {};
  std::chrono::duration<float> time_min[2] = { std::chrono::duration<float>::max(),
    std::chrono::duration<float>::max() };
  std::chrono::duration<float> time_max[2] = { std::chrono::duration<float>::min(),
    std::chrono::duration<float>::min() };
  const int benchmark_n = 1000;
  std::cout << "Vulkan compute many runs\n";
  for (int run = 0; run < benchmark_n; run++)
  {
    for (int i = 0; i < 2; i++)
    {
      BuildCommandBufferSimple(m_cmdBufferCompute, m_pipelines[i]);
      auto start = std::chrono::high_resolution_clock::now();
      VK_CHECK_RESULT(vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_fence));
      VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 100000000000));
      auto end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<float> time = end - start;
      time_min[i] = std::min(time_min[i], time);
      time_max[i] = std::max(time_max[i], time);
      time_sum[i] += time;
      vkResetFences(m_device, 1, &m_fence);
    }

  }
  std::cout << "-> Min time:                       NonShared: " << time_min[0].count() << "s\n";
  std::cout << "                                      Shared: " << time_min[1].count() << "s\n";
  std::cout << "-> Max time:                       NonShared: " << time_max[0].count() << "s\n";
  std::cout << "                                      Shared: " << time_max[1].count() << "s\n";
  std::cout << "-> Shared memory acceleration (mean):         " << time_sum[0] / time_sum[1] << std::endl;
}
