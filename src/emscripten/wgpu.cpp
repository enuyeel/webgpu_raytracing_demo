#include "wgpu.h"

//#include <emscripten/html5_webgpu.h> //emscripten_webgpu_get_device()
#include <emscripten/html5.h>        //emscripten_set_element_css_size()
#include <cstdio>                    //printf
#include <cassert>                   //assert

static WGPUSwapChain swapChain = 0;
static WGPUDevice    device = 0;

bool webgpu::createDevice(WGPUBackendType type0, WGPUBackendType type1, window::Handle handle)
{
  // Left as null (until supported in Emscripten)
  //static const WGPUInstance instance = nullptr;

  wgpuInstanceRequestAdapter(0, 0, 
    [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
    {
      if (message) {
        printf("wgpuInstanceRequestAdapter: %s\n", message);
      }

      if (status == WGPURequestAdapterStatus_Unavailable)
      {
        printf("WebGPU unavailable; exiting cleanly\n");
        // exit(0) (rather than emscripten_force_exit(0)) ensures there is no dangling keepalive.
        exit(0);
      }
      assert(status == WGPURequestAdapterStatus_Success);

      wgpuAdapterRequestDevice(adapter, nullptr, 
        [](WGPURequestDeviceStatus status, WGPUDevice dev, const char* message, void* userdata)
        {
          if (message) {
            printf("wgpuAdapterRequestDevice: %s\n", message);
          }
          assert(status == WGPURequestDeviceStatus_Success);

          device = dev;

          //wgpu::Device device = wgpu::Device::Acquire(dev);
          //reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
        }, 0);

    }, 0);

  //device = emscripten_webgpu_get_device();

  wgpuDeviceSetUncapturedErrorCallback(device,
    [](WGPUErrorType errorType, const char* message, void*) { printf("%d: %s\n", errorType, message); }, 0);

  return true;
}

//[https://zenn.dev/kd_gamegikenblg/articles/a5a8effe43bf3c]
//[https://github.com/kainino0x/webgpu-cross-platform-demo/blob/main/main.cpp]
WGPUDevice webgpu::getDevice()
{
  //// Left as null (until supported in Emscripten)
  ////static const WGPUInstance instance = nullptr;

  //wgpuInstanceRequestAdapter(0, 0, 
  //  [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
  //  {
  //    if (message) {
  //      printf("wgpuInstanceRequestAdapter: %s\n", message);
  //    }

  //    if (status == WGPURequestAdapterStatus_Unavailable)
  //    {
  //      printf("WebGPU unavailable; exiting cleanly\n");
  //      // exit(0) (rather than emscripten_force_exit(0)) ensures there is no dangling keepalive.
  //      exit(0);
  //    }
  //    assert(status == WGPURequestAdapterStatus_Success);

  //    wgpuAdapterRequestDevice(adapter, nullptr, 
  //      [](WGPURequestDeviceStatus status, WGPUDevice dev, const char* message, void* userdata)
  //      {
  //        if (message) {
  //          printf("wgpuAdapterRequestDevice: %s\n", message);
  //        }
  //        assert(status == WGPURequestDeviceStatus_Success);

  //        device = dev;

  //        //wgpu::Device device = wgpu::Device::Acquire(dev);
  //        //reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
  //      }, 0);

  //  }, 0);

  return device;
}

WGPUSwapChain webgpu::createSwapChain()
{
  if (swapChain) return swapChain;

  //TODO : ChainedStruct serves as an extension to the base struct?
  //wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
  WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
  canvasDesc.selector = "#canvas";
  canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    
  //* 'Surface' abstracts the native platform surface or window.
  WGPUSurfaceDescriptor surfaceDesc{};
  surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvasDesc);
  //'instance' is 0 for Emscripten build.
  WGPUSurface surface = wgpuInstanceCreateSurface(0, &surfaceDesc);
    
  //* 'Swap Chain' will let us rotate through the images being displayed on the canvas,
  //  rendering to a buffer which is not visible while another is shown (i.e., double-buffering).
  WGPUSwapChainDescriptor swapChainDesc{}; //wgpu::SwapChainDescriptor scDesc{};
  //* output color attachment image 
  swapChainDesc.usage = WGPUTextureUsage_RenderAttachment; //swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
  swapChainDesc.format = WGPUTextureFormat_BGRA8Unorm; //swapChainDesc.format = wgpu::TextureFormat::BGRA8Unorm;
  swapChainDesc.width = 512;
  swapChainDesc.height = 512;
  swapChainDesc.presentMode = WGPUPresentMode_Fifo; //swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
  swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);

  //TODO : Set the initial dimension of the canvas to the size of the initial w, h.
  emscripten_set_element_css_size("#canvas", swapChainDesc.width, swapChainDesc.height);

  return swapChain;
}