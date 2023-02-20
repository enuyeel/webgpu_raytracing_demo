//* References
//* [https://www.w3.org/TR/WGSL]
//* [https://www.w3.org/TR/webgpu]
//* [https://web.dev/gpu-compute/]
//* [https://alain.xyz/blog/raw-webgpu]
//* [https://www.willusher.io/graphics/2021/08/29/0-to-gltf-triangle]
//* [https://www.willusher.io/graphics/2021/08/30/0-to-gltf-bind-groups]
//* [https://community.khronos.org/t/noob-difference-between-unorm-and-srgb/106132/3]
//* [https://austin-eng.com/webgpu-samples/samples/imageBlur#main.ts]

//* Intersection
//* [https://iquilezles.org/articles/intersectors/]

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <webgpu/webgpu.h>

#else

//TODO: ?
//#ifndef _WIN32_WINNT
//#define _WIN32_WINNT 0x0601
//#endif
//#include <SDKDDKVer.h>
#include <Windows.h>
//#include <tchar.h>

#include <SDL2-2.26.2/include/SDL.h>
#include <SDL2-2.26.2/include/SDL_syswm.h> //SDL_SysWMinfo

#include <dawn/include/webgpu/webgpu.h>
#include <dawn/include/webgpu/webgpu_cpp.h> //wgpu::AdapterProperties, etc.

#include <dawn/include/dawn/dawn_proc.h> //dawnProcSetProcs()

#include <dawn/include/dawn/native/NullBackend.h>

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
#include <SDL2-2.26.2/include/SDL_vulkan.h>
#endif

static SDL_Window* window = 0;
static HWND                hWindow = 0;
static int                 windowWidth = 640;
static int                 windowHeight = 480;

static dawn::native::Instance      dInstance;
static WGPUBackendType             backendType;
static DawnSwapChainImplementation dSwapChainImplementation;
static WGPUTextureFormat           swapChainPreferredFormat;
static VkSurfaceKHR                surface = VK_NULL_HANDLE;

#endif

//#undef NDEBUG
#include <cassert> //assert()
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <fstream> //ifstream

// Left as null (until supported in Emscripten)
static const WGPUInstance  instance = 0;
static WGPUDevice          device = 0; //static wgpu::Device device;
static WGPUQueue           queue = 0; //static wgpu::Queue queue;
//static wgpu::Buffer readbackBuffer;
static WGPUComputePipeline computePipeline = 0;
static WGPURenderPipeline  fullQuadPipeline = 0; //static wgpu::RenderPipeline pipeline;
//static int testsCompleted = 0;
static WGPUSwapChain       swapChain = 0; //static wgpu::SwapChain swapChain;
static WGPUBindGroup       computeBindGroup = 0;
static WGPUBuffer          uniformIteration = 0;
static WGPUBindGroup       fullQuadBindGroup = 0;

//static const char* computeShader = 0;
//static const char* fullQuadShader = 0;

static bool eventLoop()
{
  SDL_Event event;

  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_QUIT:
      { return false; } break;

      case SDL_KEYDOWN:
      { if (event.key.keysym.sym == SDLK_ESCAPE) return false; } break;

      case SDL_WINDOWEVENT:
      {
        if (event.window.windowID == SDL_GetWindowID(window))
        {
          switch (event.window.event)
          {
            case SDL_WINDOWEVENT_CLOSE:
            { return false; } break;

            case SDL_WINDOWEVENT_SIZE_CHANGED:
            { SDL_GL_GetDrawableSize(window, &windowWidth, &windowHeight); } break;

            default: break;
          }
        }
      } break;
      
      default:
        break;
    }
  }

  return true;
}

static bool initDevice(/*void (*callback)(WGPUDevice)*/) 
{

#ifdef __EMSCRIPTEN__

   device = emscripten_webgpu_get_device();

#else

  dInstance.DiscoverDefaultAdapters();
  std::vector<dawn::native::Adapter> adapters = dInstance.GetAdapters();
  wgpu::AdapterProperties properties;
  dawn::native::Adapter* pAdapter = 0;
  for (std::vector<dawn::native::Adapter>::iterator i = adapters.begin(); i != adapters.end(); ++i)
  {
    i->GetProperties(&properties);
    //TODO: OpenGL / DirectX depending on the requested backends? Vulkan by default.
    if (WGPUBackendType_Vulkan == static_cast<WGPUBackendType>(properties.backendType))
    {
      pAdapter = &(*i);
      break;
    }
  }

  if (pAdapter)
  {
    backendType = static_cast<WGPUBackendType>(properties.backendType); //WGPUBackendType_Vulkan
    device = pAdapter->CreateDevice();
  }

#endif

  return device;
}

static bool initSwapChain()
{

#ifdef __EMSCRIPTEN__

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
  swapChainDesc.width = windowWidth;
  swapChainDesc.height = windowHeight;
  swapChainDesc.presentMode = WGPUPresentMode_Fifo; //swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
  swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
  //TODO : Set the initial dimension of the canvas to the size of the initial w, h.
  emscripten_set_element_css_size("#canvas", swapChainDesc.width, swapChainDesc.height);

#else

  //TODO: lifecycle & can we create this repeatedly?
  if (surface != VK_NULL_HANDLE)
    return false;

  //VkSurfaceKHR surface = VK_NULL_HANDLE;
  //VkWin32SurfaceCreateInfoKHR info = {};
  //info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  //info.hinstance = GetModuleHandle(NULL);
  //info.hwnd = hWindow;
  //vkCreateWin32SurfaceKHR(dawn::native::vulkan::GetInstance(device), &info, nullptr, &surface);

  if(!SDL_Vulkan_CreateSurface(window, dawn::native::vulkan::GetInstance(device), &surface))
    return false;

  if (surface == VK_NULL_HANDLE)
    return false;

  //TODO: 'userData' determines its initialization status?
  if (dSwapChainImplementation.userData != 0)
    return false;

  dSwapChainImplementation = dawn::native::vulkan::CreateNativeSwapChainImpl(device, surface);
  swapChainPreferredFormat = dawn::native::vulkan::GetNativeSwapChainPreferredFormat(&dSwapChainImplementation);

  WGPUSwapChainDescriptor swapDesc = {};
  /*
   * Currently failing (probably because the nextInChain needs setting up, and
   * also with the correct WGPUSType_* for the platform).
   *
  swapDesc.usage  = WGPUTextureUsage_OutputAttachment;
  swapDesc.format = impl::swapPref;
  swapDesc.width  = 800;
  swapDesc.height = 450;
  swapDesc.presentMode = WGPUPresentMode_Mailbox;
   */
  swapDesc.implementation = reinterpret_cast<uint64_t>(&dSwapChainImplementation);
  swapChain = wgpuDeviceCreateSwapChain(device, 0, &swapDesc);
  /*
   * Currently failing on hi-DPI (with Vulkan).
   */
  wgpuSwapChainConfigure(swapChain, swapChainPreferredFormat, WGPUTextureUsage_RenderAttachment, windowWidth, windowHeight);

#endif

  return swapChain;
}

static bool SDLInit()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
  {
    //[https://emscripten.org/docs/porting/Debugging.html#manual-print-debugging]
    printf("Failed to initialize SDL.\nError: %s\n", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("WebGPU Raytracing Demo",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    windowWidth, windowHeight,
    //TODO: OpenGL / DirectX depending on the requested backends? Vulkan by default.
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN// | SDL_WINDOW_ALLOW_HIGHDPI
  );

  if (!window)
  {
    printf("Failed to create SDL window.\nError: %s\n", SDL_GetError());
    return false;
  }

  SDL_SysWMinfo info;
  //[https://wiki.libsdl.org/SDL2/SDL_GetWindowWMInfo]  
  //"The caller must initialize the info structure's version by using SDL_VERSION(&info.version), 
  //and then this function will fill in the rest of the structure with information about the given window."
  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window, &info);
  hWindow = info.info.win.window;

  if (!hWindow)
  {
    printf("Failed to retrieve handle to window.");
    return false;
  }

  return true;
}

static bool readShader(const char* filename, std::vector<char>& buffer)
{
  std::ifstream file(filename, std::ios::ate);

  if (!file.is_open())
  {
    printf("Failed to open the shader %s.", filename);
    return false;
  }

  if (file.tellg() == -1)
  {
    printf("Failed to retrieve the position of the current character.");
    return false;
  }

  size_t length = file.tellg();
  buffer.resize(length + 1, '\0');
  file.seekg(0, file.beg);
  file.read(buffer.data(), length);
  file.close();

  return true;
}

static bool initPipeline() {

#ifdef __EMSCRIPTEN__
  wgpuDeviceSetUncapturedErrorCallback(device,
    [](WGPUErrorType errorType, const char* message, void*) { printf("%d: %s\n", errorType, message); }, 0);
#endif

  //* A 'Queue' allows you to send works asynchronously to the GPU.
  queue = wgpuDeviceGetQueue(device);

  std::vector<char> compute;
  std::vector<char> frag;
  if (!readShader("./compute.wgsl", compute) || !readShader("./frag.wgsl", frag))
    return false;

  WGPUShaderModuleWGSLDescriptor wgslDescs[2]{};
  wgslDescs[0].source = frag.data();
  wgslDescs[0].chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDescs[1].source = compute.data();
  wgslDescs[1].chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;

  WGPUShaderModuleDescriptor shaderModuleDescs[2]{};
  shaderModuleDescs[0].nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDescs[0]);
  shaderModuleDescs[1].nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDescs[1]);

  //* 'Shader Modules' are plain text WGSL files 
  //  that execute on the GPU when executing a given pipeline.
  WGPUShaderModule shaderModules[2]{};
  shaderModules[0] = wgpuDeviceCreateShaderModule(device, &shaderModuleDescs[0]);
  shaderModules[1] = wgpuDeviceCreateShaderModule(device, &shaderModuleDescs[1]);

  //TODO : Release shader?
  //wgpuShaderModuleRelease(shaderModule);

  WGPUTextureDescriptor textureDesc{};
  textureDesc.usage = 
  //* [https://www.w3.org/TR/webgpu/#namespacedef-gputextureusage]
  //* The texture can be used as the destination of a copy or write operation.
                      WGPUTextureUsage_CopyDst | 
  //* The texture can be bound for use as a sampled texture in a shader
  //  (Example: as a bind group entry for a GPUTextureBindingLayout.)
                      WGPUTextureUsage_TextureBinding |
                      WGPUTextureUsage_StorageBinding;
  textureDesc.dimension = WGPUTextureDimension_2D;
  textureDesc.size.width = 512;
  textureDesc.size.height = 512;
  textureDesc.size.depthOrArrayLayers = 1;
  textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
  textureDesc.mipLevelCount = 1;
  textureDesc.sampleCount = 1;
  textureDesc.viewFormatCount = 0;
  //* "Specifies what view format (...) allowed when calling createView()."
  //* "Adding a format have a significant performance impact, (...) avoid
  //* adding formats unnecessarily."
  textureDesc.viewFormats = 0;
  WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDesc);

  //[https://www.w3.org/TR/webgpu/#textures]
  //* "One texture consists of one or more texture subresources, (...) 
  //  identified by a mipmap level and, (...), array layer and aspect."
  //* "A GPUTextureView is a view onto some subset of the texture-
  //* subresources defined by a particular GPUTexture."
  WGPUTextureViewDescriptor textureViewDesc{};
  textureViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
  textureViewDesc.dimension = WGPUTextureViewDimension_2D;
  //* "The subresource in level 0 has the dimensions of the texture itself.
  //* These are typically used to represent levels of detail of a texture."
  textureViewDesc.baseMipLevel = 0;
  textureViewDesc.mipLevelCount = 1;
  textureViewDesc.baseArrayLayer = 0;
  textureViewDesc.arrayLayerCount = 1;
  //* "All available aspects of the texture format will be accessible
  //* (...). For color formats the color aspect will be accessible."
  textureViewDesc.aspect = WGPUTextureAspect_All; 
  WGPUTextureView textureView = wgpuTextureCreateView(texture, &textureViewDesc);

  {
    /*
      Texture (Device + TextureDescriptor)
        TextureView (Texture + TextureViewDescriptor)

      BindGroup (Device + BindGroupDescriptor)
        BindGroupLayout (Device + BindGroupLayoutDescriptor)
          BindGroupLayoutEntry(ies); only the layout information, not actual data
            [Texture, Sampler, Buffer, ...]BindingLayout
        BindGroupEntry(ies); actual data?
          TextureView, Sampler, Buffer

      ComputePipeline (Device + PipelineDescriptor)
        PipelineLayout (Device + PipelineLayoutDescriptor)
          BindGroupLayout
        ProgrammableStage
          ShaderModule
    */

    //@group(0) @binding(0) var outputTexture : texture_storage_2d<rgba8unorm, write>;
    WGPUStorageTextureBindingLayout storageTextureBindingLayout{};
    storageTextureBindingLayout.access = WGPUStorageTextureAccess_WriteOnly; //* default
    storageTextureBindingLayout.format = WGPUTextureFormat_RGBA8Unorm;
    storageTextureBindingLayout.viewDimension = WGPUTextureViewDimension_2D; //* corresponding WGSL types: (...), texture_storage_2d

    //@group(0) @binding(1) var<uniform> iteration : u32;
    WGPUBufferBindingLayout bufferBindingLayout{};
    bufferBindingLayout.type = WGPUBufferBindingType_Uniform;
    bufferBindingLayout.hasDynamicOffset = false;
    bufferBindingLayout.minBindingSize = 0;

    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[2]{};
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Compute;
    bindGroupLayoutEntries[0].storageTexture = storageTextureBindingLayout;
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Compute;
    bindGroupLayoutEntries[1].buffer = bufferBindingLayout;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;

    //WGPUBindGroupLayout bindGroupLayout = {};
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    WGPUComputePipelineDescriptor computePipelineDesc{};

    WGPUPipelineLayoutDescriptor computePipelineLayoutDesc{};
    computePipelineLayoutDesc.bindGroupLayoutCount = 1;
    computePipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;

    computePipelineDesc.layout = wgpuDeviceCreatePipelineLayout(device, &computePipelineLayoutDesc);

    WGPUProgrammableStageDescriptor programmableStageDesc{};
    programmableStageDesc.entryPoint = "main";
    programmableStageDesc.constants = 0;
    programmableStageDesc.constantCount = 0;
    programmableStageDesc.module = shaderModules[1];

    computePipelineDesc.compute = programmableStageDesc;

    //static WGPUComputePipeline computePipeline = 0;
    computePipeline = wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);

    WGPUBindGroupDescriptor bindGroupDescriptor{};
    bindGroupDescriptor.layout = bindGroupLayout;
    bindGroupDescriptor.entryCount = 2;

    //@group(0) @binding(1) var<uniform> iteration : u32;
    WGPUBufferDescriptor bufferDesc{};
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform; //* The buffer can be used as the destination of a copy or write operation.
    bufferDesc.size = 4;
    bufferDesc.mappedAtCreation = false; //TODO: ?
    uniformIteration = wgpuDeviceCreateBuffer(device, &bufferDesc);
    uint32_t iteration = 0;
    wgpuQueueWriteBuffer(queue, uniformIteration, 0, &(iteration), 4); //TODO: ?

    WGPUBindGroupEntry bindGroupEntries[2]{};
    bindGroupEntries[0].binding = 0;
    bindGroupEntries[0].textureView = textureView;
    bindGroupEntries[1].binding = 1;
    bindGroupEntries[1].buffer = uniformIteration;
    bindGroupEntries[1].offset = 0;
    bindGroupEntries[1].size = 4;
    bindGroupDescriptor.entries = bindGroupEntries;
    
    computeBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
  }

  {
    /*
      Texture (Device + TextureDescriptor)
        TextureView (Texture + TextureViewDescriptor)

      BindGroup (Device + BindGroupDescriptor)
        BindGroupLayout (Device + BindGroupLayoutDescriptor)
          BindGroupLayoutEntry(ies); only the layout information, not actual data
            [Texture, Sampler, Buffer, ...]BindingLayout
        BindGroupEntry(ies); actual data?
          TextureView, Sampler, Buffer

      RenderPipeline (Device + PipelineDescriptor)
        PipelineLayout (Device + PipelineLayoutDescriptor)
          BindGroupLayout
        FragmentState
          ShaderModule
          ColorTargetState
        VertexState
          ShaderModule
        PrimitiveState
        DepthStencilState
        MultisampleState
    */

    WGPURenderPipelineDescriptor fullQuadPipelineDescriptor{};
    //wgpu::RenderPipelineDescriptor renderPipelineDescriptor{};

    //@group(0) @binding(0) var mySampler : sampler;
    //* [https://www.w3.org/TR/webgpu/#dictdef-gpusamplerbindinglayout]
    WGPUSamplerBindingLayout samplerBindingLayout{};
    samplerBindingLayout.type = WGPUSamplerBindingType_Filtering; //* default

    //@group(0) @binding(1) var myTexture : texture_2d<f32>;
    //* [https://www.w3.org/TR/webgpu/#dictdef-gputexturebindinglayout]
    WGPUTextureBindingLayout textureBindingLayout{};
    textureBindingLayout.sampleType = WGPUTextureSampleType_Float; //* default
    textureBindingLayout.viewDimension = WGPUTextureViewDimension_2D; //* default
    textureBindingLayout.multisampled = false; //* default

    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[2]{};
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[0].sampler = samplerBindingLayout;
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[1].texture = textureBindingLayout;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    WGPUBindGroupLayout bindGroupLayout{};
    bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    fullQuadPipelineDescriptor.layout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    //* Vertex Shader Stage
    WGPUVertexState vertexState{};
    vertexState.module = shaderModules[0];
    vertexState.entryPoint = "main_v";
    vertexState.constantCount = 0;
    vertexState.bufferCount = 0;
    fullQuadPipelineDescriptor.vertex = vertexState;
    //descriptor.vertex.module = shaderModule;
    //descriptor.vertex.entryPoint = "main_v";

    //* Primitive State :
    //* Rasterization : How does the rasterizer behave when executing this graphics pipeline? Does it cull faces? Which direction should the face be culled?
    fullQuadPipelineDescriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    //renderPipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    fullQuadPipelineDescriptor.primitive.cullMode = WGPUCullMode_None;
    fullQuadPipelineDescriptor.primitive.frontFace = WGPUFrontFace_CCW;
    fullQuadPipelineDescriptor.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

    //* Depth / Stencil State
    fullQuadPipelineDescriptor.depthStencil = 0;

    //* Multisample State
    fullQuadPipelineDescriptor.multisample.nextInChain = 0;
    fullQuadPipelineDescriptor.multisample.count = 1;
    fullQuadPipelineDescriptor.multisample.mask = 0xFFFFFFFF;
    fullQuadPipelineDescriptor.multisample.alphaToCoverageEnabled = false;

    //* Fragment Shader Stage
    WGPUFragmentState fragmentState{};
    fragmentState.module = shaderModules[0];
    fragmentState.entryPoint = "main_f";
    fragmentState.targetCount = 1;
    //fragmentState.constantCount = 0;

    //* Color / Blend State
    WGPUColorTargetState colorTargetState{};
    colorTargetState.format = WGPUTextureFormat_BGRA8Unorm;
    colorTargetState.writeMask = WGPUColorWriteMask_All;
    //colorTargetState.blend = 0;

    fragmentState.targets = &colorTargetState;

    fullQuadPipelineDescriptor.fragment = &fragmentState;

    fullQuadPipeline = wgpuDeviceCreateRenderPipeline(device, &fullQuadPipelineDescriptor);

    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Nearest;
    samplerDesc.minFilter = WGPUFilterMode_Nearest;
    samplerDesc.mipmapFilter = WGPUFilterMode_Nearest;
    samplerDesc.lodMinClamp = 0.f;
    samplerDesc.lodMaxClamp = 1000.f;
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;
    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);

    WGPUBindGroupDescriptor bindGroupDescriptor{};
    bindGroupDescriptor.layout = bindGroupLayout;

    bindGroupDescriptor.entryCount = 2;
    WGPUBindGroupEntry bindGroupEntries[2]{};
    bindGroupEntries[0].binding = 0;
    bindGroupEntries[0].sampler = sampler;
    bindGroupEntries[1].binding = 1;
    bindGroupEntries[1].textureView = textureView;
    bindGroupDescriptor.entries = bindGroupEntries;
    
    fullQuadBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
  }

  return true;
}

static void render() {

  WGPUTextureView backBuffer = wgpuSwapChainGetCurrentTextureView(swapChain);

  //* color attachment
  WGPURenderPassColorAttachment colorAttachment{};
  colorAttachment.view = backBuffer;
  //colorAttachment.loadOp = wgpu::LoadOp::Clear;
  colorAttachment.loadOp = WGPULoadOp_Clear;
  //colorAttachment.storeOp = wgpu::StoreOp::Store;
  colorAttachment.storeOp = WGPUStoreOp_Store;
  colorAttachment.clearValue = {0.2, 0.5, 0.75, 1};
  
  //* The render pass descriptor specifies the images to bind to the targets that will be written to in the fragment shader, and optionally a depth buffer and the occlusion query set.
  //wgpu::RenderPassDescriptor renderpassDesc{};
  WGPURenderPassDescriptor renderpassDesc{};
  renderpassDesc.colorAttachmentCount = 1;
  renderpassDesc.colorAttachments = &colorAttachment;

  //* depth stencil attachment is omitted here

  //wgpu::CommandBuffer commands;
  //* In contrast to Metal, WebGPU does not require you to explicitly create a command buffer prior to encoding commands. (...) you create a command encoder directly from the device, and when you’re ready to submit a batch of commands for execution, you call the finish() function on the encoder. This function returns a command buffer that can be submitted to the device’s command queue.
  
  //wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  //ComputePassEncoder encoder.BeginComputePass();
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, 0);
  //wgpuCommandEncoderBeginComputePass(encoder, 0);

  //wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpassDescriptor);
  WGPUComputePassEncoder computePassEncoder = wgpuCommandEncoderBeginComputePass(encoder, 0);
  wgpuComputePassEncoderSetPipeline(computePassEncoder, computePipeline);
  wgpuComputePassEncoderSetBindGroup(computePassEncoder, 0, computeBindGroup, 0, 0);
  wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, 32, 32, 1);
  wgpuComputePassEncoderEnd(computePassEncoder);
  wgpuComputePassEncoderRelease(computePassEncoder);

  WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &renderpassDesc);
  //pass.SetPipeline(pipeline);
  wgpuRenderPassEncoderSetPipeline(renderPassEncoder, fullQuadPipeline);
  wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, fullQuadBindGroup, 0, 0);
  //pass.Draw(3, 1, 0, 0);
  //* uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
  wgpuRenderPassEncoderDraw(renderPassEncoder, 6, 1, 0, 0);
  //pass.End();
  wgpuRenderPassEncoderEnd(renderPassEncoder);
  //! pass.EndPass();
  wgpuRenderPassEncoderRelease(renderPassEncoder);

  //WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, 0);
  //commands = encoder.Finish();
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, 0);

  static uint32_t callCount = 1;
  wgpuQueueWriteBuffer(queue, uniformIteration, 0, &callCount, 4);
  ++callCount;

  wgpuQueueSubmit(queue, 1, &commands);
  //queue.Submit(1, &commands);
  wgpuCommandBufferRelease(commands);

  wgpuTextureViewRelease(backBuffer);
}

static void loop()
{
  while (1)
  {
    if (!eventLoop()) break;

    render();
  }
}

int main(int, char* []) {

#ifndef DAWN_ENABLE_BACKEND_VULKAN
  return -1;
#endif

  if (!SDLInit())
    return -1;

  if (!initDevice())
    return -1;

  //* Needs this before calling any functions starting with prefix 'wgpu'.
  DawnProcTable procs(dawn::native::GetProcs());
  procs.deviceSetUncapturedErrorCallback(device, 
    [](WGPUErrorType errorType, const char* message, void*){ printf("%d: %s\n", errorType, message); }, 0);
  dawnProcSetProcs(&procs);

  if (!initSwapChain())
    return -1;

  if (!initPipeline())
    return -1;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(render, 0, false);
#endif

  loop();

  wgpuBindGroupRelease(computeBindGroup);
  wgpuBindGroupRelease(fullQuadBindGroup);
  wgpuBufferRelease(uniformIteration);
  wgpuComputePipelineRelease(computePipeline);
  wgpuRenderPipelineRelease(fullQuadPipeline);
  wgpuSwapChainRelease(swapChain);
  wgpuQueueRelease(queue);
  wgpuDeviceRelease(device);

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
