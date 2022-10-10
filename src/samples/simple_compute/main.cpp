#include <chrono>
#include <functional>
#include <random>
#include <numeric>
#include "simple_compute.h"

constexpr int LENGTH           = 100'000'000;
constexpr int VULKAN_DEVICE_ID = 0;

template <typename F, typename ...Args>
auto benchmark(F&& f, Args&&... args)
{
  auto start = std::chrono::steady_clock::now();
  std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> elapsed = end - start;
  return elapsed.count();
}

auto vulkan_init()
{
  std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(LENGTH);
  if (app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return app;
  }

  app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

  return app;
}

auto cpu_init()
{
  std::vector<float> values(LENGTH);
  std::random_device rd;
  std::mt19937 gen(0xdeadbeef);
  std::uniform_real_distribution<> dis(-10000.0f, 10000.0f);
  for (uint32_t i = 0; i < values.size(); ++i)
    values[i] = (float)dis(gen);

  return values;
}

auto vulkan_run()
{
  auto app = vulkan_init();
  app->Execute();
}

void cpu_run()
{
  auto values = cpu_init();
  std::vector<float> res = std::vector<float>(LENGTH);
  auto start = std::chrono::steady_clock::now();
  int window = 7;
  int offset = window / 2;
  auto size  = values.size();
  for (int i = 0; i < size; i++)
  {
    float sum = 0.0f;
    for (int j = 0; j < window; j++)
    {
      int id = i + j - offset;
      if (id >= 0 && id < size)
      {
        sum += values[id];
      }
    }
    res[i] = values[i] - sum / 7;
  }
  auto end_comp = std::chrono::steady_clock::now();
  float mean = std::accumulate(std::begin(res), std::end(res), 0.0) / values.size();
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> elapsed_compute = end_comp - start;
  std::chrono::duration<float> elapsed_total = end - start;
  std::cout << "CPU\n";
  std::cout << "-> Result:                                    " << mean << std::endl;
  std::cout << "-> Compute time:                              " << elapsed_compute.count() << "s\n";
  std::cout << "-> Compute + Calc mean time:                  " << elapsed_total.count() << "s";
}
// На моем пк следующий диапазон значений
//Vulkan
//-> Result : -1.93579e-05
//-> Compute time : 0.017s-0.1s
//-> Compute + Reading buffer + Calc mean time : 1.5-1.7s
//Vulkan shared memory->Result : -1.93579e-05
//-> Compute time : 0.009s-0.09s
//-> Compute + Reading buffer + Calc mean time : 1.5-1.7s
//CPU
//-> Result : -1.93586e-05
//-> Compute time : 4.5-4.6s
//-> Compute + Calc mean time : 4.7-5.0s

// Реализация с использованием вулкана примерно в 3 раза быстрее чем на cpu.
// Непосредственно время исполнения обычного шейдера и шейдера с shared памятью довольно сильно скачет.
// Ускорение при первом прогоне shared шейдера варьируется от 1.2 до 3.
// Однако в среднем при большом количестве прогонов ускорения практически сходит на нет.
int main()
{
  vulkan_run();
  cpu_run();
  return 0;
}
