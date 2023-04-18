#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec3 position;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec3 albedo;

layout (location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 texCoord;
} vsOut;

layout (push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint albedoId;
} PushConstant;

layout (binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

void main()
{
  position = (Params.view * vec4(vsOut.wPos, 1.0)).xyz;
  normal = (Params.view * vec4(vsOut.wNorm, 0.0)).xyz;
  switch(PushConstant.albedoId)
  {
  case 0:
    albedo = vec3(0.57f, 0.4f, 0.65f);
    break;
  case 1:
    albedo = vec3(0.29f, 0.44f, 0.65f);
    break;
  case 2:
    albedo = vec3(0.3f, 0.5f, 0.32f);
    break;
  case 3:
    albedo = vec3(0.73f, 0.8f, 0.19f);
    break;
  case 4:
    albedo = vec3(0.31f, 0.25f, 0.85f);
    break;
  case 5:
    albedo = vec3(1.0f, 0.0f, 0.0f);
    break;
  }
}
