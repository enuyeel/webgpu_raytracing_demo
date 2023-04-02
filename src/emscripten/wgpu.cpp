#include "wgpu.h"

#include <emscripten/html5_webgpu.h> //emscripten_webgpu_get_device()
#include <emscripten/html5.h>        //emscripten_set_element_css_size()

static WGPUSwapChain swapChain = 0;

bool webgpu::createDevice(WGPUBackendType type0, WGPUBackendType type1, window::Handle handle)
{
  return true;
}

WGPUDevice webgpu::getDevice()
{
  return emscripten_webgpu_get_device();
}

WGPUSwapChain webgpu::createSwapChain()
{
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
  WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(emscripten_webgpu_get_device(), surface, &swapChainDesc);

  //TODO : Set the initial dimension of the canvas to the size of the initial w, h.
  emscripten_set_element_css_size("#canvas", swapChainDesc.width, swapChainDesc.height);

  return swapChain;
}