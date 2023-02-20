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