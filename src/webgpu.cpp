#include "webgpu.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <dawn/include/webgpu/webgpu_cpp.h>       //wgpu::AdapterProperties, etc.
#include <dawn/include/dawn/dawn_proc.h>          //dawnProcSetProcs()
#include <dawn/include/dawn/native/NullBackend.h> //dawn::native::null::

#ifndef __has_include
#define __has_include(header) 0
#endif

/*
  [https://stackoverflow.com/questions/152016/detecting-cpu-architecture-compile-time]
  #if defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
  #elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
  return "x86_32";
*/

#if __has_include("vulkan/vulkan.h") && \
    (defined(__x86_64__) || defined(_M_X64) || \
     defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86))
#define DAWN_ENABLE_BACKEND_VULKAN
#endif

#ifdef DAWN_ENABLE_BACKEND_VULKAN
#include <dawn/native/VulkanBackend.h>
#include <vulkan/vulkan_win32.h> //VkWin32SurfaceCreateInfoKHR
#endif

//TODO:
#if __has_include("d3d12.h") || (_MSC_VER >= 1900)
#define DAWN_ENABLE_BACKEND_D3D12
#endif

#ifdef DAWN_ENABLE_BACKEND_D3D12
#include <dawn/native/D3D12Backend.h>
#endif

static dawn::native::Instance      dInstance;
static DawnSwapChainImplementation dSwapChainImplementation;
static WGPUTextureFormat           swapChainPreferredFormat;
static VkSurfaceKHR                surface = VK_NULL_HANDLE;

static WGPUSwapChain               swapChain = 0; //static wgpu::SwapChain swapChain;
static WGPUBackendType             backendType;
static WGPUDevice                  device = 0; //static wgpu::Device device;

//TODO: currently returns false if 'dSwapChainImplementation', and 'surface' is initialized
static bool initSwapChain(
  //WGPUBackendType backendType, WGPUDevice device
  window::Handle handle
)
{
  //TODO: 'userData' determines its initialization status?
  if (dSwapChainImplementation.userData != 0)
    return false;

  //#ifdef __EMSCRIPTEN__
  //
  //  //TODO : ChainedStruct serves as an extension to the base struct?
  //  //wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
  //  WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
  //  canvasDesc.selector = "#canvas";
  //  canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
  //
  //  //* 'Surface' abstracts the native platform surface or window.
  //  WGPUSurfaceDescriptor surfaceDesc{};
  //  surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvasDesc);
  //  //'instance' is 0 for Emscripten build.
  //  WGPUSurface surface = wgpuInstanceCreateSurface(0, &surfaceDesc);
  //
  //  //* 'Swap Chain' will let us rotate through the images being displayed on the canvas,
  //  //  rendering to a buffer which is not visible while another is shown (i.e., double-buffering).
  //  WGPUSwapChainDescriptor swapChainDesc{}; //wgpu::SwapChainDescriptor scDesc{};
  //  //* output color attachment image 
  //  swapChainDesc.usage = WGPUTextureUsage_RenderAttachment; //swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
  //  swapChainDesc.format = WGPUTextureFormat_BGRA8Unorm; //swapChainDesc.format = wgpu::TextureFormat::BGRA8Unorm;
  //  swapChainDesc.width = windowWidth;
  //  swapChainDesc.height = windowHeight;
  //  swapChainDesc.presentMode = WGPUPresentMode_Fifo; //swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
  //  swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
  //  //TODO : Set the initial dimension of the canvas to the size of the initial w, h.
  //  emscripten_set_element_css_size("#canvas", swapChainDesc.width, swapChainDesc.height);
  //
  //#else

  switch (backendType)
  {

#ifdef DAWN_ENABLE_BACKEND_VULKAN
    case WGPUBackendType_Vulkan:
    {
      //TODO: lifecycle & can we create this repeatedly?
      if (surface != VK_NULL_HANDLE)
        return false;

      VkWin32SurfaceCreateInfoKHR info = {};
      info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      info.hinstance = GetModuleHandle(NULL);
      info.hwnd = reinterpret_cast<HWND>(handle);
      if (vkCreateWin32SurfaceKHR(dawn::native::vulkan::GetInstance(device), &info, nullptr, &surface) != VK_SUCCESS)
        return false;
      surface = surface;

      dSwapChainImplementation = dawn::native::vulkan::CreateNativeSwapChainImpl(device, surface);
      swapChainPreferredFormat = dawn::native::vulkan::GetNativeSwapChainPreferredFormat(&dSwapChainImplementation);
    } break;
#endif

#ifdef DAWN_ENABLE_BACKEND_D3D12
    case WGPUBackendType_D3D12:
    {
      dSwapChainImplementation = dawn::native::d3d12::CreateNativeSwapChainImpl(device, reinterpret_cast<HWND>(handle));
      swapChainPreferredFormat = dawn::native::d3d12::GetNativeSwapChainPreferredFormat(&dSwapChainImplementation);
    } break;
#endif

    default:

      dSwapChainImplementation = dawn::native::null::CreateNativeSwapChainImpl();
      swapChainPreferredFormat = WGPUTextureFormat_Undefined;

      break;
  }

  return true;
}

bool webgpu::createDevice(
  WGPUBackendType type0, WGPUBackendType type1, 
  window::Handle handle
)
{

//#ifdef __EMSCRIPTEN__
//
//  device = emscripten_webgpu_get_device();
//
//#else

  dInstance.DiscoverDefaultAdapters();
  std::vector<dawn::native::Adapter> adapters = dInstance.GetAdapters();
  wgpu::AdapterProperties properties;
  dawn::native::Adapter* pAdapter0 = 0;
  dawn::native::Adapter* pAdapter1 = 0;

  for (std::vector<dawn::native::Adapter>::iterator i = adapters.begin();
       i != adapters.end(); ++i)
  {
    i->GetProperties(&properties);

    if (!pAdapter0 && type0 == static_cast<WGPUBackendType>(properties.backendType))
      pAdapter0 = &(*i);

    if (!pAdapter1 && type1 == static_cast<WGPUBackendType>(properties.backendType))
      pAdapter1 = &(*i);

    if (pAdapter0 && pAdapter1) break;
  }

  //empty adapter wrapper if not the best choice adapter
  dawn::native::Adapter adapter = dawn::native::Adapter();

  if (pAdapter0)
    adapter = *pAdapter0;
  else if (pAdapter1)
    adapter = *pAdapter1;

  adapter.GetProperties(&properties);
  backendType = static_cast<WGPUBackendType>(properties.backendType);
  device = adapter.CreateDevice();

  if (!initSwapChain(handle)) return false;

  //* Needs this before calling any functions starting with prefix 'wgpu'.
  DawnProcTable procs(dawn::native::GetProcs());
  procs.deviceSetUncapturedErrorCallback(device,
    [](WGPUErrorType errorType, const char* message, void*) { printf("%d: %s\n", errorType, message); }, 0);
  dawnProcSetProcs(&procs);

//#endif

  return true;
}

WGPUDevice webgpu::getDevice()
{
  return device;
}

WGPUSwapChain webgpu::createSwapChain()
{
  WGPUSwapChainDescriptor swapDesc = {};
  swapDesc.implementation = reinterpret_cast<uintptr_t>(&dSwapChainImplementation);
  swapChain = wgpuDeviceCreateSwapChain(device, 0, &swapDesc);
  //TODO: fails with Vulkan
  wgpuSwapChainConfigure(swapChain, swapChainPreferredFormat, WGPUTextureUsage_RenderAttachment, 512, 512);

  return swapChain;
}