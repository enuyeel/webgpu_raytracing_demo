#include <dawn/include/webgpu/webgpu.h>

#include "window.h" //window::Handle

namespace webgpu
{
  bool createDevice(WGPUBackendType type0, WGPUBackendType type1, window::Handle handle);

  WGPUDevice getDevice();

  WGPUSwapChain createSwapChain();
}