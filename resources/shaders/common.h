#ifndef VK_GRAPHICS_BASIC_COMMON_H
#define VK_GRAPHICS_BASIC_COMMON_H

// GLSL-C++ datatype compatibility layer

#ifdef __cplusplus

#include <LiteMath.h>

// NOTE: This is technically completely wrong,
// as GLSL words are guaranteed to be 32-bit,
// while C++ unsigned int can be 16-bit.
// Touching LiteMath is forbidden though, so yeah.
using shader_uint  = LiteMath::uint;
using shader_uvec2 = LiteMath::uint2;
using shader_uvec3 = LiteMath::uint3;

using shader_float = float;
using shader_vec2  = LiteMath::float2;
using shader_vec3  = LiteMath::float3;
using shader_vec4  = LiteMath::float4;
using shader_mat4  = LiteMath::float4x4;

// The funny thing is, on a GPU, you might as well consider
// a single byte to be 32 bits, because nothing can be smaller
// than 32 bits, so a bool has to be 32 bits as well.
using shader_bool  = LiteMath::uint;

#else

#define shader_uint  uint
#define shader_uvec2 uvec2

#define shader_float float
#define shader_vec2  vec2
#define shader_vec3  vec3
#define shader_vec4  vec4
#define shader_mat4  mat4

#define shader_bool  bool

vec3 getAlbedo(uint albedoId)
{
  switch (albedoId)
  {
  case 0:
    return vec3(0.57f, 0.4f, 0.65f);
  case 1:
    return vec3(0.99f, 0.1f, 0.2f);
  case 2:
    return vec3(0.3f, 0.9f, 0.32f);
  case 3:
    return vec3(0.93f, 0.9f, 0.19f);
  case 4:
    return vec3(0.11f, 0.25f, 0.95f);
  case 5:
    return vec3(0.2f, 0.90f, 0.90f);
  }
}

#endif


struct UniformParams
{
  shader_mat4  lightMatrix;
  shader_vec3  lightPos;
  shader_float time;
  shader_vec3  baseColor;
  shader_bool  animateLightColor;
  shader_uint  rsmSampleCount;
  shader_float rsmRadius;
  shader_bool  rsmEnabled;
  shader_float rsmIntentsity;
};

#endif // VK_GRAPHICS_BASIC_COMMON_H
