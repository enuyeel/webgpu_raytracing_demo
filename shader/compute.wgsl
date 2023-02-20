//* Normalize the u32 to [0.0, 1.0] f32
//* float unorm(uint n)
fn myUnorm(n : u32) -> f32
{ 
  return f32(n) * (1.0 / f32(0xffffffffu)); 
}

// if it turns out that you are unhappy with the distribution or performance
// it is possible to exchange this function without changing the interface
//* [https://nullprogram.com/blog/2018/07/31/]
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

const lambert    : u32 = 0;
const metal      : u32 = 1;
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
  // let f32Infinity : f32 = bitcast<f32>(0x7F800000u);
  let f32Infinity : f32 = 3.40282346638528859812e+38f;
    
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