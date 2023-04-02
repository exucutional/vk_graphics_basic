#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(binding = 1) uniform sampler2D fogDepthMap;

float hash(float n)
{
    return fract(sin(n)*43758.5453);
}

float noise(in vec3 x)
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    
    f = f*f*(3.0 - 2.0*f);
    
    float n = p.x + p.y*57.0 + 113.0*p.z;
    
    float res = mix(mix(mix(hash(n+0.0),   hash(n+1.0), f.x),
                        mix(hash(n+57.0),  hash(n+58.0), f.x), f.y),
                    mix(mix(hash(n+113.0), hash(n+114.0), f.x),
                        mix(hash(n+170.0), hash(n+171.0), f.x), f.y), f.z);
    return res;
}

float fbm(vec3 p)
{
    float f;
    f  = 0.5000*noise(p); p = p*2.02;
    f += 0.2500*noise(p); p = p*2.03;
    f += 0.1250*noise(p);
    return f + 0.3*(1.+sin(Params.time));
}

float sdf_smooth_union(float d1, float d2, float k)
{
    float h = max(k-abs(d1-d2), 0.0);
    return min(d1, d2) - h*h*0.25/k;
}

float sdf_union(float d1, float d2)
{
  return min(d1, d2);
}

float sdf_sphere(vec3 pos, float r, vec3 centre)
{
  return length(pos-centre)-r;
}

float sdf_torus(vec3 pos, vec3 centre, vec2 t)
{
  vec3 lpos = pos-centre;
  vec2 q = vec2(length(lpos.xz)-t.x, lpos.y);
  return length(q)-t.y;
}

float sdf_scene(vec3 pos)
{
  float s1 = sdf_sphere(pos, 1., vec3(0.0, -0.27+sin(Params.time), 0.0));
  float s2 = sdf_sphere(pos, 1., vec3(0.0, -1.27-sin(Params.time), 0.0));
  float t = sdf_torus(pos, vec3(0.0, -1.27, 0.0), vec2(3.0, 0.05));
  return sdf_union(sdf_smooth_union(s1, s2, 0.25), t);
}

float fog(vec3 pos)
{
  //const vec3 fogCentre = vec3(0., 0., 0.);
  return -sdf_scene(pos) + fbm(pos*3.0*(1.+0.4*sin(Params.time)));
}

float restore_view_depth(vec2 uv)
{
  vec2 depthCoord = uv*0.5 + vec2(0.5, 0.5);
  depthCoord.y = 1 - depthCoord.y;
  float sDepth = textureLod(fogDepthMap, depthCoord, 0).x;
  vec4 vPosUnScaled = Params.projInverse*vec4(uv.x, uv.y, sDepth, 1.0);
  float vDepth = -(vPosUnScaled/vPosUnScaled.w).z;
  return vDepth;
}

// https://www.shadertoy.com/view/WslGWl

vec3 calc_dir(vec2 uv)
{
  float tanFov = tan(radians(Params.fov/2));
  float x = uv.x*Params.aspect*tanFov;
  float y = uv.y*tanFov;
  vec4 viewDir = vec4(x, y, -1., 0.);
  vec4 worldDir = Params.viewInverse*viewDir;
  return worldDir.xyz;
}

void main()
{
  vec4 color = vec4(0.);
  int steps = 100;
  float rayMarchMax = 30.;
  float stepSize = rayMarchMax/steps;
  vec2 uv = vOut.texCoord * 2 - vec2(1.);
  float vDepth = restore_view_depth(uv);
  vec3 dir = calc_dir(uv);
  //vec3 dir = normalize(Params.cameraForward + Params.cameraUp*uv.y + Params.cameraRight*uv.x);
  float absorption = 100.0;
  float T = 1.0;
  float rayLength = 0.0;
  for (int step = 0; step < steps; ++step)
  {
    if (rayLength > vDepth)
      break;

    vec3 wPos = Params.cameraPos + dir*rayLength;
    float density = fog(wPos);
    if (density > 0.0)
    {
      float tmp = density/float(steps);
      T *= 1.0-(tmp*absorption);
      if (T <= 0.01)
      {
          break;
      }
      float opaity = 50.0;
      float k = opaity*tmp*T;
      vec4 cloudColor = vec4(1.0);
      vec4 col1 = cloudColor*k;
      color += col1;
    }
    rayLength += stepSize;
  }
  out_fragColor = color;
}
