#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 normal;

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
}
