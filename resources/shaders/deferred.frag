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

layout(binding = 1) uniform sampler2D gbuffer;
layout(binding = 2) uniform sampler2D fogMap;

void main()
{
  vec2 coord = vec2(vOut.texCoord.x, 1-vOut.texCoord.y);
  vec4 color = textureLod(gbuffer, coord, 0);
  vec4 fog = textureLod(fogMap, coord, 0);
  out_fragColor = color + fog;
}
