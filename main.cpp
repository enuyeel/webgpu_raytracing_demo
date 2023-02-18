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
static WGPUBuffer uniformBuffer = 0;
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
//* A value declaration creates a name for a value, and that value is immutable once it has been declared.
//* A variable declaration creates a name for memory locations for storing a value; the value stored there may be updated.
//* [https://www.w3.org/TR/WGSL/#ref-ptr-types]
//* in WGSL source text: Reference types must not appear.

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

  //* [https://www.w3.org/TR/WGSL/#address-space]
  //* Shared among invocations in the same compute shader workgroup
  //var<workgroup> globalSeed : atomic<u32>;
  //TODO : seed shared by workgroup needs its initialization & access in control?;
  //TODO : atomic store / load, barrier?
  //var<workgroup> globalSeed : u32;
  //* Shared among invocations in the same shader stage
  //@group(0) @binding(1)
  //var<storage, read_write> globalSeed : u32;
  var<private> globalSeed : u32;
  
  fn uhash() -> u32
  { 
    //var x : u32 = atomicLoad(&globalSeed);
    var x : u32 = globalSeed;
    x = x ^ (x >> 16u);
    x = x * 0x7feb352du;
    x = x ^ (x >> 15u);
    x = x * 0x846ca68bu;
    x = x ^ (x >> 16u);
    //atomicStore(&globalSeed, x);
    globalSeed = x;

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
  fn urandom() -> u32
  {
    return uhash();
  }

  //* float random(inout Random rng)
  // fn random(rng : ptr<private, Random>) -> f32
  // { 
  //   return myUnorm(urandom(rng)); 
  // }
  fn random() -> f32
  {
    return myUnorm(urandom());
  }

  //* vec2 random2(inout Random rng)
  // fn random2(rng : ptr<private, Random>) -> vec2<f32>
  // { 
  //   return vec2<f32>(random(rng),random(rng)); 
  // }
  fn random2() -> vec2<f32>
  {
    return vec2<f32>(random(), random());
  }

  // uniformly random on the surface of a sphere
  // produces normal vectors as well
  //* vec3 uniform_sphere_area (inout Random rng)
  fn uniform_sphere_area () -> vec3<f32>
  {
    //* Map the [0, 1] to [0, 2PI]; hence 'phi'
    var phi : f32 = 6.28318530718 * random();
    //* Map the [0, 1] to [-1, 1]; hence 'cos(rho)'
    var cosrho : f32 = 2.0 * random() - 1.0;
    var sinrho : f32 = sqrt(1.0 - (cosrho * cosrho));
    //* Varies depending on the coordinate system;
    return vec3<f32>(sinrho * cos(phi), cosrho, sinrho * sin(phi));
    //* original line
    //return vec3<f32>(sinrho * cos(phi), sinrho * sin(phi), cosrho);
  }

  // uniformly random within the volume of a sphere
  //* vec3 uniform_sphere_volume (inout Random rng)
  fn uniform_sphere_volume () -> vec3<f32>
  {
    //TODO : pow() is something like a pdf?; without it, points appear to be
    //TODO : clustered at the center of the sphere.
    return uniform_sphere_area() * pow(random(), 1.0/3.0);
  }

  @group(0) @binding(1) var<uniform> iteration : u32;
  @group(0) @binding(0) var outputTexture : texture_storage_2d<rgba8unorm, write>;

  const workgroupSizeX : u32 = 16;
  const workgroupSizeY : u32 = 16;
  const workgroupSizeZ : u32 = 1;

  const lambert : u32    = 0;
  const metal : u32      = 1;
  const dielectric : u32 = 2;

  fn SDSphere(poi : vec3<f32>, //* point of interest
              center : vec3<f32>, 
              radius : f32) -> f32
  {
    return length(poi - center) - radius;
  }

  struct HitRecord { 
    d : f32, 
    n : vec3<f32>,          //* surface normal
    albedo : vec3<f32>,     //* object's albedo; diffuse color
    materialType : u32,     //* object's material type
    fuzz : f32,             //* specific to metal
    isFrontFace : bool,     
    indexOfRefraction : f32 //* specific to dielectric
  };

  fn sphereIntersect( 
    ro : vec3<f32>, rd : vec3<f32>, 
    ce : vec3<f32>, r : f32, albedo : vec3<f32>, materialType : u32, 
    fuzz : f32, indexOfRefraction : f32, //* specific to certain types
    tMin : f32, tMax : f32,
    hitRecord : ptr<function, HitRecord> ) -> bool
  {
    //* (A-C); from the original equation
    let oc : vec3<f32> = ro - ce;
    //let a : f32 = dot( rd, rd ); //* assumes ray direction is normalized... for now
    //b : f32 = 2.0 * dot( oc, rd );
    let half_b : f32 = dot( oc, rd );
    let c : f32 = dot( oc, oc ) - r * r;
    //discriminant = b * b - 4 * a * c;
    //discriminant = b * b - 4 * c;
    let discriminant : f32 = half_b * half_b - c;
    //let discriminant : f32 = half_b * half_b - a * c;
    if ( discriminant < 0.0 ){ return false; }
    let sd : f32 = sqrt( discriminant );

    //var root : f32 = ( -half_b - sd ) / a;
    var root : f32 = ( -half_b - sd );
    if ( root < tMin || root > tMax )
    {
      //root = ( -half_b + sd ) / a;
      root = ( -half_b + sd );
      if ( root < tMin || root > tMax ){ return false; }
    }

    (*hitRecord).d = root;
    (*hitRecord).albedo = albedo; 
    (*hitRecord).materialType = materialType;
    (*hitRecord).fuzz = fuzz;
    (*hitRecord).indexOfRefraction = indexOfRefraction;
    
    let outwardNormal : vec3<f32> = (ro + (root * rd) - ce) / r;
    (*hitRecord).isFrontFace = dot( outwardNormal, rd ) <= 0.0;
    //(*hitRecord).n = outwardNormal;
    if ( (*hitRecord).isFrontFace ) { (*hitRecord).n = outwardNormal; }
    else { (*hitRecord).n = -outwardNormal; }

    return true;
  }

  fn scene(
    ro : vec3<f32>, rd : vec3<f32>,
    tMin : f32, tMax : f32,
    hitType : ptr<function, HitRecord> ) -> bool
  {
    //var sd : f32 = SDSphere(poi, vec3<f32>(0.0, 0.0, 5.0), 1.0);
    //sd = min(sd, SDSphere(poi, vec3<f32>(0.0, -30.0, 5.0), 29.0) );

    var isHit : bool = false;
    (*hitType).d = tMax;

    //! GOAT bug... || is a short-circuit operator, so it matters whether isHit is on the left, or on the right. Simply use '|=' to get away with subtle BS.
    isHit |= sphereIntersect(ro, rd, vec3<f32>( 0.0,    0.0, -1.0),   0.5, vec3<f32>(0.7, 0.3, 0.3), lambert, 0.0, 0.0, tMin, (*hitType).d, hitType);
    isHit |= sphereIntersect(ro, rd, vec3<f32>( 0.0, -100.5, -1.0), 100.0, vec3<f32>(0.8, 0.8, 0.0), lambert, 0.0, 0.0, tMin, (*hitType).d, hitType);
    //isHit |= sphereIntersect(ro, rd, vec3<f32>(-1.0,    0.0, -1.0),   0.5, vec3<f32>(0.8, 0.8, 0.8), metal, 0.3, 0.0, tMin, (*hitType).d, hitType);
    isHit |= sphereIntersect(ro, rd, vec3<f32>(-1.0,    0.0, -1.0),  -0.5, vec3<f32>(0.0, 0.0, 0.0), dielectric, 0.0, 1.0 / 1.5, tMin, (*hitType).d, hitType);
    isHit |= sphereIntersect(ro, rd, vec3<f32>( 1.0,    0.0, -1.0),   0.5, vec3<f32>(0.8, 0.6, 0.2), metal, 1.0, 0.0, tMin, (*hitType).d, hitType);

    return isHit;
  }

  @compute @workgroup_size(workgroupSizeX, workgroupSizeY, workgroupSizeZ)
  fn main(
    @builtin(workgroup_id) workgroupIdx : vec3<u32>,
    @builtin(local_invocation_id) localInvocationIdx : vec3<u32>,
  )
  {
    // workgroupBarrier();
    // globalSeed = workgroupIdx.x * workgroupSizeX + workgroupIdx.y * workgroupSizeY;
    // workgroupBarrier();

    // storageBarrier();
    // globalSeed = 0;
    // storageBarrier();

    //TODO : does it work as intended?
    //* [https://stackoverflow.com/questions/10435253/glsl-infinity-constant]
    let f32Infinity : f32 = bitcast<f32>(0x7F800000u);
    
    let dim : vec2<u32> = textureDimensions(outputTexture);
    let fragCoords : vec2<u32> = vec2<u32>(
      workgroupIdx.x * workgroupSizeX + localInvocationIdx.x,
      workgroupIdx.y * workgroupSizeY + localInvocationIdx.y
    );

    //TODO : profiling tool for branching performance?
    //if (fragCoords.x < 0 || fragCoords.x >= dim.x ||
    //    fragCoords.y < 0 || fragCoords.y >= dim.y)
    if (fragCoords.x >= dim.x || fragCoords.y >= dim.y)
    {
      return; //* much like 'discard'.
    }

    const e : f32 = 0.001;
    //* 50% reflector : objects only absorb half the energy on each bounce
    //TODO : different reflector for different models?
    //const reflector : f32 = 0.5;
    //* introduced while changing implementation detail from recursive to iterative func
    //var totalBounce : f32 = 1.0;
    //var totalDistance : f32 = 0.0;
    //var hitDistance : f32 = maximumDistance;
    //* Initial ray properties; rayMarch() is recursive.
    let rayOrigin : vec3<f32> = vec3<f32>(0.0, 0.0, 1.0); //* camera position
    var rayOriginPrime : vec3<f32> = rayOrigin;

    const depthMax : u32 = 50;
    //var depth : u32 = 0;
    const sampleSize : u32 = 32;
    globalSeed = u32( fragCoords.x + (fragCoords.y * dim.x) ) * iteration;
    var attenuation : vec3<f32> = vec3<f32>(1.0);

    //* pixelColor is more appropriate than fragColor?
    var pixelColor : vec3<f32> = vec3<f32>(0.0, 0.0, 0.0);
    for(var sampleIdx : u32 = 0; sampleIdx < sampleSize; sampleIdx++)
    {
      //* Normalized pixel coordinates; 
      //* x : [-aspect_ratio, aspect_ratio], y : [-1, 1]
      let pixelCoords : vec2<f32> = ( 2.0 * ( vec2<f32>(fragCoords) + vec2<f32>(random(), random()) ) - vec2<f32>(dim) ) / f32(dim.y);
      //TODO : normalize? depends on implementation of intersection funcs, etc.
      var rayDirPrime : vec3<f32> = normalize(vec3<f32>(pixelCoords, -1.0));

      for(var i : u32 = 0; i < depthMax; i++)
      {
        var hitRecord : HitRecord;
        if (scene(rayOriginPrime, rayDirPrime, e, f32Infinity, &hitRecord))
        {
          //* rayOrigin'(prime); scattered ray's origin
          rayOriginPrime = rayOriginPrime + rayDirPrime * hitRecord.d;

          //* rayDir'(prime); scattered direction
          if(hitRecord.materialType == lambert)
          {
            rayDirPrime = normalize( hitRecord.n + uniform_sphere_area() );

            //* degenerate case
            if (abs(rayDirPrime.x) < 1e-8 &&
                abs(rayDirPrime.y) < 1e-8 &&
                abs(rayDirPrime.z) < 1e-8)
            {
              rayDirPrime = hitRecord.n;
            }
          }
          else if(hitRecord.materialType == metal)
          {
            let reflected = rayDirPrime - 2.0 * dot( hitRecord.n, rayDirPrime ) * hitRecord.n;
            rayDirPrime = normalize( reflected + uniform_sphere_area() * hitRecord.fuzz );
            //TODO : 
            if (dot(rayDirPrime, hitRecord.n) <= 0.0) { break; }
          }
          else //* dielectric
          {
            hitRecord.albedo = vec3<f32>(1.0, 1.0, 1.0); //* force white color

            var indexOfRefraction = hitRecord.indexOfRefraction;
            //* if it's the ray inside the object
            if(!hitRecord.isFrontFace) { indexOfRefraction = 1.0 / indexOfRefraction; }

            let cosTheta = dot( -rayDirPrime, hitRecord.n );
            let sinTheta = sqrt( 1.0 - cosTheta * cosTheta );

            //* No solution to the Snell's law; thus cannot reflect, and must reflect
            if (indexOfRefraction * sinTheta > 1.0) 
            {
              rayDirPrime = rayDirPrime - 2.0 * dot( hitRecord.n, rayDirPrime ) * hitRecord.n;
            }
            else
            {
              //* t = t_\perp+t_\parallel 

              //* t_\perp=\frac{n_1}{n_2}\left \{ i-(i\cdot n)n \right \}
              let tPerp : vec3<f32> = indexOfRefraction * ( rayDirPrime - dot(rayDirPrime, hitRecord.n) * hitRecord.n );

              //* t_\parallel=(t \cdot n)n
              //* t_\parallel=\pm\sqrt{1-(\frac{n_1}{n_2})^2\left \{1-(i\cdot n)^2\right \}}n
              let tPar : vec3<f32> = -sqrt( 1 - dot( tPerp, tPerp ) ) * hitRecord.n;

              rayDirPrime = tPerp + tPar;
            }
          }

          attenuation *= hitRecord.albedo;

          continue;
        }
        else
        {
          let t : f32 = 0.5 * (rayDirPrime.y + 1.0);
          //TODO : This line replaces texture color lookup for light sources.
          //* Linear interpolation between white, and sky blue;
          //* straight up vector (0, 1, 0) will yield 100% sky blue color
          let light : vec3<f32> = (1.0 - t) * vec3<f32>(1.0, 1.0, 1.0) + t * vec3<f32>(0.5, 0.7, 1.0);

          //pixelColor += totalBounce * light;
          pixelColor += attenuation * light;
          break;
        }
      }

      //! Reinitialize for clean sheet
      //totalBounce = 1.0;
      attenuation = vec3<f32>(1.0);
      rayOriginPrime = rayOrigin;
    }

    pixelColor *= 1.0 / f32(sampleSize);
    //TODO : progressive refinement instead of multiple sampling is an option.
    textureStore(outputTexture, fragCoords, vec4<f32>(sqrt(pixelColor), 1.0));
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

  WGPUBufferDescriptor bufferDesc{};
  bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform; //* The buffer can be used as the destination of a copy or write operation.
  bufferDesc.size = 4; //* The size of the buffer in bytes.
  bufferDesc.mappedAtCreation = false; 
  //WGPUBuffer uniformBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
  uniformBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
  //wgpuBufferGetMappedRange(uniformBuffer, 0, 4);
  //wgpuBufferUnmap(uniformBuffer);
  //TODO : Filler; seemingly backend has problem w/ not initializing the buffer (D3D12 command allocator Reset() E_FAIL)
  wgpuQueueWriteBuffer(queue, uniformBuffer, 0, 0, 4); 

  {
    WGPUComputePipelineDescriptor computePipelineDesc{};

    /*
      @group(0) @binding(1) var<uniform> iteration : u32;
      @group(0) @binding(0) var outputTexture : texture_storage_2d<rgba8unorm, write>;
    */

    WGPUStorageTextureBindingLayout storageTextureBindingLayout{};
    storageTextureBindingLayout.access = WGPUStorageTextureAccess_WriteOnly; //* default
    storageTextureBindingLayout.format = WGPUTextureFormat_RGBA8Unorm;
    storageTextureBindingLayout.viewDimension = WGPUTextureViewDimension_2D; //* corresponding WGSL types: (...), texture_storage_2d

      WGPUBufferBindingLayout bufferBindingLayout{};
      bufferBindingLayout.type = WGPUBufferBindingType_Uniform;
      bufferBindingLayout.hasDynamicOffset = false;
      bufferBindingLayout.minBindingSize = 0;

      // WGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
      // bindGroupLayoutEntry.binding = 0;
      // bindGroupLayoutEntry.visibility = WGPUShaderStage_Compute;
      // bindGroupLayoutEntry.storageTexture = storageTextureBindingLayout;

      WGPUBindGroupLayoutEntry bindGroupLayoutEntries[2] = {};
      bindGroupLayoutEntries[0].binding = 0;
      bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Compute;
      bindGroupLayoutEntries[0].storageTexture = storageTextureBindingLayout;
      bindGroupLayoutEntries[1].binding = 1;
      bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Compute;
      bindGroupLayoutEntries[1].buffer = bufferBindingLayout;

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};

      bindGroupLayoutDesc.entryCount = 2;
      bindGroupLayoutDesc.entries = bindGroupLayoutEntries;

      // bindGroupLayoutDesc.entryCount = 1;
      // bindGroupLayoutDesc.entries = &bindGroupLayoutEntry;

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

      // bindGroupDescriptor.entryCount = 1;

      bindGroupDescriptor.entryCount = 2;

      // WGPUBindGroupEntry bindGroupEntry{};
      // bindGroupEntry.binding = 0;
      // bindGroupEntry.textureView = textureView;
      // bindGroupDescriptor.entries = &bindGroupEntry;

      WGPUBindGroupEntry bindGroupEntries[2];
      bindGroupEntries[0].binding = 0;
      bindGroupEntries[0].textureView = textureView;
      bindGroupEntries[1].binding = 1;
      bindGroupEntries[1].buffer = uniformBuffer;
      bindGroupEntries[1].offset = 0;
      bindGroupEntries[1].size = 4;
      bindGroupDescriptor.entries = bindGroupEntries;
    
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

  static uint32_t callCount = 1;
  wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &callCount, 4);
  ++callCount;

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
    //swapChainDesc.width = 800;
    //swapChainDesc.height = 600;
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
