#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout (binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D hdrImage;

float luminance(vec4 v)
{
    return dot(v, vec4(0.2126f, 0.7152f, 0.0722f, 1.0f));
}

// https://www.shadertoy.com/view/4dBcD1
vec4 reinhard_jodie(vec4 v)
{
    float l = luminance(v);
    vec4 tv = v / (1.0f + v);
    return mix(v / (1.0f + l), tv, tv);
}

void main()
{
  const vec4 hdrColor = texture(hdrImage, surf.texCoord);
  if (Params.toneMapping)
  {
    out_fragColor = reinhard_jodie(hdrColor);
  }
  else
  {
    out_fragColor = clamp(hdrColor, vec4(0.0f), vec4(1.0f));
  }
}
