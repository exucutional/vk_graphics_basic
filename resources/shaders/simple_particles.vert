#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) in vec4 color;
layout(location = 1) in vec2 position;
layout(location = 2) in vec2 velocity;
layout(location = 3) in float time;

layout( push_constant ) uniform params {
  float pointSize;
} PushConstant;

layout (location = 0 ) out VS_OUT
{
    vec4 color;
} vOut;

out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };

void main(void)
{
    vOut.color = color;
    gl_PointSize = PushConstant.pointSize;
    if (time > 0.0f)
    {
      gl_Position = vec4(position / 10.0f, 0.0f, 1.0f);
    }
    else
    {
      gl_Position = vec4(-2.0, -2.0, -2.0, -2.0); // Clip not active particles away
    }
}
