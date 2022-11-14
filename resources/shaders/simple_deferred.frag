#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 normal;
layout(location = 1) out vec4 albedo;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint albedoId;
} PushConstant;

layout (location = 0) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

void main()
{
  normal = vec4(surf.wNorm, 1.0);
  switch(PushConstant.albedoId)
  {
  case 0:
    albedo = vec4(0.57f, 0.4f, 0.65f, 1.0f);
    break;
  case 1:
    albedo = vec4(0.29f, 0.44f, 0.65f, 1.0f);
    break;
  case 2:
    albedo = vec4(0.3f, 0.5f, 0.32f, 1.0f);
    break;
  case 3:
    albedo = vec4(0.73f, 0.8f, 0.19f, 1.0f);
    break;
  case 4:
    albedo = vec4(0.31f, 0.25f, 0.85f, 1.0f);
    break;
  case 5:
    albedo = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    break;
  }
}
