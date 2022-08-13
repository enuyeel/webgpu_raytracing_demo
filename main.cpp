// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

//#ifdef __EMSCRIPTEN__
#include <webgpu/webgpu.h>
//#endif

#undef NDEBUG
#include <cassert> //assert()
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

// Left as null (until supported in Emscripten)
static const WGPUInstance instance = 0;
//static wgpu::Device device;
static WGPUDevice device = 0;
//static wgpu::Queue queue;
static WGPUQueue queue = 0;
//static wgpu::Buffer readbackBuffer;
static WGPUComputePipeline computePipeline = 0;
static WGPURenderPipeline fullQuadPipeline = 0;
//static wgpu::RenderPipeline pipeline;
//static int testsCompleted = 0;
//static wgpu::SwapChain swapChain;
static WGPUSwapChain swapChain = 0;
static WGPUBindGroup computeBindGroup = 0;
static WGPUBindGroup fullQuadBindGroup = 0;

void GetDevice(void (*callback)(WGPUDevice)) 
{
  /*
    GetDevice(
      [](wgpu::Device dev) 
      {
        device = dev;
        run();
      }
    );
  */

  //! Current method is the most up-to-date method to retrieve device.
  //* emscripten_webgpu_get_device() is an alternative way to get device?
  //* [https://github.com/emscripten-core/emscripten/issues/11359#issuecomment-642247789]
  //* [https://github.com/emscripten-core/emscripten/issues/15172#issuecomment-930662060]
  // device = emscripten_webgpu_get_device();
  // if (device)
  // {
  //   callback(device);
  // }

  //* An 'Adapter' describes the physical properties of a given GPU,
  //* such as its name, extensions, and device limits.
  wgpuInstanceRequestAdapter(instance, 0, 
    //* 4th param 'userdata' binds to the callback function it seems.
    [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) 
    {
      assert(status == WGPURequestAdapterStatus_Success);
      //* Retrieving the adapter is successful, but querying its properties isn't possible? 
      //WGPUAdapterProperties properties;
      //wgpuAdapterGetProperties(adapter, &properties);
      //printf("vendorID : %d\ndeviceID : %d\n", properties.vendorID, properties.deviceID);
      //printf("name : %s\nproperties : %s\n", properties.name, properties.driverDescription);

      //* A 'Device' is how you access the core of the WebGPU API, 
      //* and will allow you to create the data structures you'll need.
      wgpuAdapterRequestDevice(adapter, nullptr, 
        [](WGPURequestDeviceStatus status, WGPUDevice dev, const char* message, void* userdata) 
        {
          assert(status == WGPURequestDeviceStatus_Success);
          //device = dev;
          //wgpu::Device device = wgpu::Device::Acquire(dev);
          reinterpret_cast<void (*)(WGPUDevice)>(userdata)(dev);
        }, 
      userdata);
    }, 
    reinterpret_cast<void*>(callback)
  );
}

/*

  //UNORM is linear and is accessed directly; SRGB is stored non-linearly and is converted to linear upon access.
  //A storage texture supports accessing a single texel without the use of a sampler.
  @group(0) @binding(0) var outputTexture : texture_storage_2d<rgba8unorm, write>;

  @compute
  fn main_compute() {

    //fn textureStore(t: texture_storage_2d<F,write>, coords: vec2<C>, value: vec4<CF>)
    //C is i32, or u32
    //CF (channel format) depends on the storage texel format
    textureStore(outputTexture, vec2<u32>(0, 0), vec4<f32>(1.0, 1.0, 1.0, 1.0));
  }
*/

//TODO : Address space definition?
//* Address spaces
//* [https://www.w3.org/TR/WGSL/#address-space]
//* [https://www.w3.org/TR/WGSL/#var-and-value]

//* [https://www.w3.org/TR/WGSL/#var-and-value]
//* A value declaration creates a name for a value, and that value is immutable once it has been declared.
//* A variable declaration creates a name for memory locations for storing a value; the value stored there may be updated.

//* [https://www.shadertoy.com/view/ssGXDd]

//* The maximum value of the product of the workgroup_size dimensions for a compute stage GPUShaderModule entry-point.
//* [https://www.w3.org/TR/webgpu/#dom-supported-limits-maxcomputeinvocationsperworkgroup]

//* [https://www.w3.org/TR/WGSL/#builtin-values]

//TODO : Taking advantage of the texture sampling hardware?
//* [https://www.w3.org/TR/webgpu/#dom-gpucomputepassencoder-dispatchworkgroups]
//* (...) if a GPUShaderModule defines an entry point with @workgroup_size(4, 4), (...) call computePass.dispatchWorkgroups(8, 8); the entry point will be invoked 1024 times total: Dispatching a 4x4 workgroup 8 times along both the X and Y axes. (4*4*8*8=1024)
static const char computeShader[] = R"(

  // struct Random { 
  //   s0 : u32,
  //   s1 : u32,
  // };

  //* Normalize the u32 to [0.0, 1.0] f32
  //* float unorm(uint n)
  fn myUnorm(n : u32) -> f32
  { 
    return f32(n) * (1.0 / f32(0xffffffffu)); 
  }

  // if it turns out that you are unhappy with the distribution or performance
  // it is possible to exchange this function without changing the interface
  //* [https://nullprogram.com/blog/2018/07/31/]
  //* uint uhash(uint a, uint b)
  // fn uhash(a : u32, b : u32) -> u32
  // { 
  //   var x : u32 = ((a * 1597334673u) ^ (b * 3812015801u));
  //   x = x ^ (x >> 16u);
  //   x = x * 0x7feb352du;
  //   x = x ^ (x >> 15u);
  //   x = x * 0x846ca68bu;
  //   x = x ^ (x >> 16u);
  //   return x;
  // }

  fn uhash(seed : u32) -> u32
  { 
    var x : u32 = seed;
    x = x ^ (x >> 16u);
    x = x * 0x7feb352du;
    x = x ^ (x >> 15u);
    x = x * 0x846ca68bu;
    x = x ^ (x >> 16u);
    return x;
  }

  // fundamental functions to fetch a new random number
  // the last static call to the rng will be optimized out
  //* uint urandom(inout Random rng)
  // fn urandom(rng : ptr<private, Random>) -> u32
  // {
  //   var last : u32 = (*rng).s1;
  //   var next : u32 = uhash((*rng).s0, (*rng).s1);
  //   (*rng).s0 = (*rng).s1; 
  //   (*rng).s1 = next;
  //   return last;
  // }

  fn urandom(seed : u32) -> u32
  {
    return uhash(seed);
  }

  //* float random(inout Random rng)
  // fn random(rng : ptr<private, Random>) -> f32
  // { 
  //   return myUnorm(urandom(rng)); 
  // }
  fn random(seed : u32) -> f32
  {
    return myUnorm(urandom(seed));
  }

  //* vec2 random2(inout Random rng)
  // fn random2(rng : ptr<private, Random>) -> vec2<f32>
  // { 
  //   return vec2<f32>(random(rng),random(rng)); 
  // }
  fn random2(seed1 : u32, seed2 : u32) -> vec2<f32>
  {
    return vec2<f32>(random(seed1), random(seed2));
  }

  // uniformly random on the surface of a sphere
  // produces normal vectors as well
  //* vec3 uniform_sphere_area (inout Random rng)
  fn uniform_sphere_area (seed1 : u32, seed2 : u32) -> vec3<f32>
  {
    var u : vec2<f32> = random2(seed1, seed2);
    //* Map the [0, 1] to [0, 2PI]; hence 'phi'
    var phi : f32 = 6.28318530718 * u.x;
    //* Map the [0, 1] to [-1, 1]; hence 'cos(rho)'
    var cosrho : f32 = 2.0 * u.y - 1.0;
    var sinrho : f32 = sqrt(1.0 - (cosrho * cosrho));
    //* Varies depending on the coordinate system;
    return vec3<f32>(sinrho * cos(phi), cosrho, sinrho * sin(phi));
    //* original line
    //return vec3<f32>(sinrho * cos(phi), sinrho * sin(phi), cosrho);
  }

  // uniformly random within the volume of a sphere
  //* vec3 uniform_sphere_volume (inout Random rng)
  fn uniform_sphere_volume (seed1 : u32, seed2 : u32, seed3 : u32) -> vec3<f32>
  {
    //TODO : pow() is something like a pdf?; without it, points appear to be
    //TODO : clustered at the center of the sphere.
    return uniform_sphere_area(seed1, seed2) * pow(random(seed3), 1.0/3.0);
  }

  @group(0) @binding(0) var outputTexture : texture_storage_2d<rgba8unorm, write>;

  const workgroup_size_x : u32 = 16;
  const workgroup_size_y : u32 = 16;
  const workgroup_size_z : u32 = 1;

  fn SDSphere(poi : vec3<f32>, //* point of interest
              center : vec3<f32>, 
              radius : f32) -> f32
  {
    return length(poi - center) - radius;
  }

  fn scene(poi : vec3<f32>) -> f32
  {
    var sd : f32 = SDSphere(poi, vec3<f32>(0.0, 0.0, 5.0), 1.0);
    sd = min(sd, SDSphere(poi, vec3<f32>(0.0, -30.0, 5.0), 29.0) );

    return sd;
  }

  fn rayMarch(
    //pixelCoords : vec2<f32>
    rayOrigin : vec3<f32>,
    rayDir : vec3<f32>,
    fragCoords : vec2<u32>, dim : vec2<u32>,
    //depth : u32
  ) -> vec3<f32>
  {
    const e : f32 = 0.0001;
    const maximumDistance : f32 = 1000.0;
    const maximumIteration : u32 = 1000;
    //* 50% reflector : objects only absorb half the energy on each bounce
    //TODO : different reflector for different models?
    const reflector : f32 = 0.5;
    var totalBounce : f32 = 1.0;
    var totalDistance : f32 = 0.0;
    //var hitDistance : f32 = maximumDistance;
    var rayOriginPrime : vec3<f32> = rayOrigin;
    var rayDirPrime : vec3<f32> = rayDir;

    const depthMax : u32 = 50;
    var depth : u32 = 0;

    for(var i : u32 = 0; i < maximumIteration; i++)
    {
      if (depth >= depthMax)
      {
        return vec3<f32>(0.0);
      }

      let rayEnd : vec3<f32> = rayOriginPrime + rayDirPrime * totalDistance;

      let distance : f32 = scene(rayEnd);   
      totalDistance += distance;

      //TODO : mitigate shadow acne; ray trapped inside the objects?
      if (distance <= e)
      {
        //hitDistance = totalDistance;

        //* rayOrigin'(prime)
        rayOriginPrime = rayOriginPrime + rayDirPrime * totalDistance;

        //surface normal vector
        let N : vec3<f32> = normalize(
          vec3<f32>(scene(rayOriginPrime + vec3( e, 0., 0.)) 
                  - scene(rayOriginPrime - vec3( e, 0., 0.)),
                    scene(rayOriginPrime + vec3(0.,  e, 0.)) 
                  - scene(rayOriginPrime - vec3(0.,  e, 0.)),
                    scene(rayOriginPrime + vec3(0., 0.,  e)) 
                  - scene(rayOriginPrime - vec3(0., 0.,  e)))
        );

        //TODO : proper name adhering to the compute shader standards
        var temp : u32 = fragCoords.x + (fragCoords.y * dim.x);
        //* rayDir'(prime)
        rayDirPrime = rayOriginPrime + N + uniform_sphere_area(
          //TODO : 1,061,683,200 (FHD Texture) reaching 4,294,967,295; 
          //TODO : approximately 2,071.261 samples (depth)
          temp + (dim.x * dim.y) * depth, 
          //TODO : 2,071.261 >> 2 for convergence
          0xffffffffu - temp, 
        );
        depth++;
        rayDirPrime = normalize(rayDirPrime - rayOriginPrime);

        //* Reinitialize for clean sheet
        totalBounce *= reflector;
        totalDistance = 0.0;
        i = 0;

        continue;
      }

      if (totalDistance >= maximumDistance)
      {
        //hitDistance = maximumDistance;
        break;
      }
    } 

    let rayDirNorm : vec3<f32> = normalize(rayDirPrime);
    let t : f32 = 0.5 * (rayDirNorm.y + 1.0);
    //TODO : This line replaces texture color lookup for light sources.
    //* Linear interpolation between white, and sky blue;
    //* straight up vector (0, 1, 0) will yield 100% sky blue color
    let light : vec3<f32> = (1.0 - t) * vec3<f32>(1.0, 1.0, 1.0) + t * vec3<f32>(0.5, 0.7, 1.0);

    return totalBounce * light;
  }

  @compute @workgroup_size(workgroup_size_x, workgroup_size_y, workgroup_size_z)
  fn main(
    @builtin(workgroup_id) workgroupIdx : vec3<u32>,
    @builtin(local_invocation_id) localInvocationIdx : vec3<u32>,
  )
  {  
    let dim : vec2<i32> = textureDimensions(outputTexture);
    let fragCoords : vec2<i32> = vec2<i32>(
      i32(workgroupIdx.x * workgroup_size_x + localInvocationIdx.x),
      i32(workgroupIdx.y * workgroup_size_y + localInvocationIdx.y)
    );

    //TODO : profiling tool for branching performance?
    //if (fragCoords.x < 0 || fragCoords.x >= dim.x ||
    //    fragCoords.y < 0 || fragCoords.y >= dim.y)
    if (fragCoords.x >= dim.x || fragCoords.y >= dim.y)
    {
      return; //* much like 'discard'.
    }
    //* Normalized pixel coordinates; 
    //* x : [-aspect_ratio, aspect_ratio], y : [-1, 1]
    let pixelCoords : vec2<f32> = (2.0 * vec2<f32>(fragCoords) - vec2<f32>(dim)) / f32(dim.y);

    //* Initial ray properties; rayMarch() is recursive.
    let rayOrigin : vec3<f32> = vec3<f32>(0.0, 0.0, 0.0); //* camera position
    let rayDir : vec3<f32> = normalize(vec3<f32>(pixelCoords, 1.0)); //* No need to normalize.
    
    let finalColor : vec3<f32> = rayMarch(rayOrigin, rayDir, 
      vec2<u32>(fragCoords), 
      vec2<u32>(dim));

    textureStore(outputTexture, fragCoords, vec4<f32>(finalColor, 1.0));
  }

)";

//* Drawing a full screen quad; cool trick but it's hard to calculate UVs.
//* [https://stackoverflow.com/questions/2588875/whats-the-best-way-to-draw-a-fullscreen-quad-in-opengl-3-2]

//* @binding()
//* [https://gpuweb.github.io/gpuweb/wgsl/#attribute-binding]
//* Must only be applied to a resource variable.
//* [https://gpuweb.github.io/gpuweb/wgsl/#resource]
//* Uniform buffers, Storage buffers, Texture resources, Sampler resources
static const char fullQuadShader[] = R"(

  @group(0) @binding(0) var mySampler : sampler;
  @group(0) @binding(1) var myTexture : texture_2d<f32>;

  struct vOutput {
    @builtin(position) position : vec4<f32>,
    @location(0) uv : vec2<f32>,
  }

  @vertex
  fn main_v(@builtin(vertex_index) idx: u32) -> vOutput
  {
    var pos = array<vec2<f32>, 6>
    (
      //1st triangle
      vec2<f32>(-1.0,  1.0), 
      vec2<f32>(-1.0, -1.0), 
      vec2<f32>( 1.0, -1.0),

      //2nd triangle
      vec2<f32>( 1.0, -1.0),
      vec2<f32>( 1.0,  1.0),
      vec2<f32>(-1.0,  1.0)
    );

    var uv = array<vec2<f32>, 6>
    (
      //1st triangle
      vec2<f32>(0.0, 1.0), 
      vec2<f32>(0.0, 0.0), 
      vec2<f32>(1.0, 0.0),

      //2nd triangle
      vec2<f32>(1.0, 0.0),
      vec2<f32>(1.0, 1.0),
      vec2<f32>(0.0, 1.0)
    );

    var output : vOutput;
    output.position = vec4<f32>(pos[idx], 0.0, 1.0);
    output.uv = uv[idx];
    return output;
  }

  @fragment
  fn main_f(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> 
  {
    return textureSample(myTexture, mySampler, uv);
  }

)";

void init() {

  // WGPUSupportedLimits limits{};
  // wgpuDeviceGetLimits(device, &limits);
  // // if(wgpuDeviceGetLimits(device, &limits))
  // // {
  //   /*
  //     uint32_t maxComputeWorkgroupStorageSize;
  //     uint32_t maxComputeInvocationsPerWorkgroup;
  //     uint32_t maxComputeWorkgroupSizeX;
  //     uint32_t maxComputeWorkgroupSizeY;
  //     uint32_t maxComputeWorkgroupSizeZ;
  //     uint32_t maxComputeWorkgroupsPerDimension;
  //   */
  //   printf("Workgroup Size (x, y, z) = (%d, %d, %d)\n", limits.limits.maxComputeWorkgroupSizeX, limits.limits.maxComputeWorkgroupSizeY, limits.limits.maxComputeWorkgroupSizeZ);
  // //}

  wgpuDeviceSetUncapturedErrorCallback(device, 
    [](WGPUErrorType errorType, const char* message, void*) 
    {
      //printf("%d: %s\n", errorType, message);
    }, 0);

  //* A 'Queue' allows you to send works asynchronously to the GPU.
  //queue = device.GetQueue();
  queue = wgpuDeviceGetQueue(device);

  //TODO : direct-list-initialization
  WGPUShaderModuleWGSLDescriptor wgslDescs[2] = {};
  wgslDescs[0].source = fullQuadShader;
  wgslDescs[0].chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDescs[1].source = computeShader;
  wgslDescs[1].chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;

  WGPUShaderModuleDescriptor shaderModuleDescs[2] = {};
  shaderModuleDescs[0].nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDescs[0]);
  shaderModuleDescs[1].nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDescs[1]);

  //* 'Shader Modules' are plain text WGSL files 
  //* that execute on the GPU when executing a given pipeline.
  WGPUShaderModule shaderModules[2] = {};
  shaderModules[0] = wgpuDeviceCreateShaderModule(device, &shaderModuleDescs[0]);
  shaderModules[1] = wgpuDeviceCreateShaderModule(device, &shaderModuleDescs[1]);

  //TODO : Release shader?
  //wgpuShaderModuleRelease(shaderModule);

  WGPUTextureDescriptor textureDesc{};
  textureDesc.usage = 
  //* [https://www.w3.org/TR/webgpu/#namespacedef-gputextureusage]
  //* The texture can be used as the destination of a copy or write operation.
                      WGPUTextureUsage_CopyDst | 
  //* The texture can be bound for use as a sampled texture in a shader (Example: as a bind group entry for a GPUTextureBindingLayout.)
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
  WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDesc);

  WGPUTextureViewDescriptor textureViewDesc{};
  textureViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
  textureViewDesc.dimension = WGPUTextureViewDimension_2D;
  textureViewDesc.baseMipLevel = 0;
  //TODO : Shouldn't exceed (view's) texture's mipLevelCount?
  textureViewDesc.mipLevelCount = 1;
  textureViewDesc.baseArrayLayer = 0;
  //textureViewDesc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
  //TODO : Same for the depthOrArrayLayers?
  textureViewDesc.arrayLayerCount = 1;
  //* All available aspects of the texture format will be accessible to the texture view. For color formats the color aspect will be accessible.
  textureViewDesc.aspect = WGPUTextureAspect_All; 
  WGPUTextureView textureView = wgpuTextureCreateView(texture, &textureViewDesc);

  {
    WGPUComputePipelineDescriptor computePipelineDesc{};

    WGPUStorageTextureBindingLayout storageTextureBindingLayout{};
    storageTextureBindingLayout.access = WGPUStorageTextureAccess_WriteOnly; //* default
    storageTextureBindingLayout.format = WGPUTextureFormat_RGBA8Unorm;
    storageTextureBindingLayout.viewDimension = WGPUTextureViewDimension_2D; //* corresponding WGSL types: (...), texture_storage_2d

    WGPUBindGroupLayoutEntry bindGroupLayoutEntry = {};
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Compute;
    bindGroupLayoutEntry.storageTexture = storageTextureBindingLayout;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries = &bindGroupLayoutEntry;
    WGPUBindGroupLayout bindGroupLayout = {};
    bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

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
    computePipeline = wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);

    WGPUBindGroupDescriptor bindGroupDescriptor{};
    bindGroupDescriptor.layout = bindGroupLayout;

    bindGroupDescriptor.entryCount = 1;
    WGPUBindGroupEntry bindGroupEntry;
    bindGroupEntry.binding = 0;
    bindGroupEntry.textureView = textureView;
    bindGroupDescriptor.entries = &bindGroupEntry;
    
    computeBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
  }

  {
    WGPURenderPipelineDescriptor fullQuadPipelineDescriptor{};
    //wgpu::RenderPipelineDescriptor renderPipelineDescriptor{};

    //* [https://www.w3.org/TR/webgpu/#dictdef-gpusamplerbindinglayout]
    WGPUSamplerBindingLayout samplerBindingLayout{};
    samplerBindingLayout.type = WGPUSamplerBindingType_Filtering; //* default

    //* [https://www.w3.org/TR/webgpu/#dictdef-gputexturebindinglayout]
    WGPUTextureBindingLayout textureBindingLayout{};
    textureBindingLayout.sampleType = WGPUTextureSampleType_Float; //* default
    textureBindingLayout.viewDimension = WGPUTextureViewDimension_2D; //* default
    textureBindingLayout.multisampled = false; //* default

    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[2] = {};
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[0].sampler = samplerBindingLayout;
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[1].texture = textureBindingLayout;

    //* Uniform Data
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    WGPUBindGroupLayout bindGroupLayout{};
    bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    fullQuadPipelineDescriptor.layout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    //wgpu::PipelineLayoutDescriptor pl{};
    //renderPipelineDescriptor.layout = device.CreatePipelineLayout(&pl);

    //* Vertex Shader Stage
    WGPUVertexState vertexState{};
    //wgpu::VertexState vertexState{};
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
    //descriptor.multisample;

    //* Fragment Shader Stage
    WGPUFragmentState fragmentState{};
    //wgpu::FragmentState fragmentState{};
    fragmentState.module = shaderModules[0];
    fragmentState.entryPoint = "main_f";
    fragmentState.targetCount = 1;
    //fragmentState.constantCount = 0;

    //* Color / Blend State
    WGPUColorTargetState colorTargetState{};
    //wgpu::ColorTargetState colorTargetState{};
    //colorTargetState.format = wgpu::TextureFormat::BGRA8Unorm;
    colorTargetState.format = WGPUTextureFormat_BGRA8Unorm;
    colorTargetState.writeMask = WGPUColorWriteMask_All;

    fragmentState.targets = &colorTargetState;

    fullQuadPipelineDescriptor.fragment = &fragmentState;

    //BlendState const * blend = nullptr;

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
    WGPUBindGroupEntry bindGroupEntries[2];
    bindGroupEntries[0].binding = 0;
    bindGroupEntries[0].sampler = sampler;
    bindGroupEntries[1].binding = 1;
    bindGroupEntries[1].textureView = textureView;
    bindGroupDescriptor.entries = bindGroupEntries;
    
    fullQuadBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);
  }
}

//void render(wgpu::TextureView view) {
void render(WGPUTextureView view) {

  //* color attachment
  //wgpu::RenderPassColorAttachment colorAttachment{};
  WGPURenderPassColorAttachment colorAttachment{};
  colorAttachment.view = view;
  //colorAttachment.loadOp = wgpu::LoadOp::Clear;
  colorAttachment.loadOp = WGPULoadOp_Clear;
  //colorAttachment.storeOp = wgpu::StoreOp::Store;
  colorAttachment.storeOp = WGPUStoreOp_Store;
  //! attachment.clearColor = {0, 0, 0, 1};
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
  
  wgpuQueueSubmit(queue, 1, &commands);
  //queue.Submit(1, &commands);
  wgpuCommandBufferRelease(commands);
}

//#ifdef __EMSCRIPTEN__

void frame() {

  WGPUTextureView backBuffer = wgpuSwapChainGetCurrentTextureView(swapChain);
  //wgpu::TextureView backbuffer = swapChain.GetCurrentTextureView();
  render(backBuffer);
}
//#endif

// EM_BOOL canvasResize(int eventType, const EmscriptenUiEvent *uiEvent, void *userData)
// {
//   printf("Inner (w, h) = (%d, %d) Outer (w, h) = (%d, %d)\n",
//     uiEvent->windowInnerWidth, uiEvent->windowInnerHeight,
//     uiEvent->windowOuterWidth, uiEvent->windowOuterHeight);

//   int wi, hi;
//   emscripten_get_canvas_element_size("#canvas", &wi, &hi);
//   printf("Canvas Element\n(w, h) = (%d, %d)\n", wi, hi);

//   double w, h;
//   emscripten_get_element_css_size("#canvas", &w, &h);
//   printf("Element CSS\n(w, h) = (%f, %f)\n", w, h);
//   return false;
// }

void run() {
  init();

  //static constexpr int kNumTests = 5;
  //doCopyTestMappedAtCreation(false);
  //doCopyTestMappedAtCreation(true);
  //doCopyTestMapAsync(false);
  //doCopyTestMapAsync(true);
  //doRenderTest();

//#ifdef __EMSCRIPTEN__
  {
    //TODO : ChainedStruct serves as an extension to the base struct?
    //wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = "#canvas";
    canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;

    //* 'Surface' abstracts the native platform surface or window.
    //wgpu::SurfaceDescriptor surfDesc{};
    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvasDesc);
    //wgpu::Instance instance{};  // null instance
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);
    //wgpu::Surface surface = instance.CreateSurface(&surfDesc);

    //* The 'swap chain' will let us rotate through the images being displayed on the canvas, rendering to a buffer which is not visible while another is shown (i.e., double-buffering).
    //wgpu::SwapChainDescriptor scDesc{};
    WGPUSwapChainDescriptor swapChainDesc{};
    //* output color attachment image
    //swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    //swapChainDesc.format = wgpu::TextureFormat::BGRA8Unorm;
    swapChainDesc.format = WGPUTextureFormat_BGRA8Unorm;
    swapChainDesc.width = 512;
    swapChainDesc.height = 512;
    //swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;
    swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
    //TODO : Set the initial dimension of the canvas to the size of the initial w, h.
    emscripten_set_element_css_size("#canvas", swapChainDesc.width, swapChainDesc.height);
    //swapChain = device.CreateSwapChain(surface, &scDesc);
  }

  //* [https://emscripten.org/docs/api_reference/html5.h.html#c.emscripten_set_resize_callback]
  // emscripten_set_resize_callback(
  //   EMSCRIPTEN_EVENT_TARGET_WINDOW, //#canvas is interpreted as a CSS query selector: “the first element with CSS ID ‘canvas’”.
  //   0, 
  //   true, //TODO : ?
  //   canvasResize);

  emscripten_set_main_loop(frame, 0, false);
//#endif
}

int main() {
  GetDevice(
    [](WGPUDevice dev) 
    {
      device = dev;
      run();
    }
  );
}
