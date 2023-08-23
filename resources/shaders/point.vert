#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) in vec3 position;

layout (location = 0 ) out vec4 color;

layout(push_constant) uniform params_t
{
  mat4 mProjView;
  mat4 mModel;
  int instanceCountSqrt;
  float pointSize;
} params;

out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };

mat4 translate4x4(float x, float y, float z)
{
  mat4 m = mat4(0.0f);
  m[0][0] = 1;
  m[1][1] = 1;
  m[2][2] = 1;
  m[3] = vec4(x/4, y/4, z/4, 1.0);
  return m;
}

void main(void)
{
  color = vec4(1.0f);
  gl_PointSize = params.pointSize;
  vec4 wPos = params.mModel*vec4(position, 1.0f);
  mat4 translate = translate4x4(gl_InstanceIndex/params.instanceCountSqrt, mod(gl_InstanceIndex, params.instanceCountSqrt), 0.0f);
  gl_Position = params.mProjView*translate*wPos;
}
