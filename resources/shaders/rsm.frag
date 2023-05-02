#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vsOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D rsmDepth;
layout (binding = 2) uniform sampler2D rsmPosition;
layout (binding = 3) uniform sampler2D rsmNormal;
layout (binding = 4) uniform sampler2D rsmFlux;

void main()
{
  const vec4 wPos = texture(rsmPosition, texCoord);
  const vec4 clipPos = Params.lightMatrix*wPos;
  const vec3 ndcPoc  = clipPos.xyz/clipPos.w;
  const vec2 shadowTexCoord = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]      
  out_fragColor   = (lightColor*shadow + vec4(0.1f)) * vec4(Params.baseColor, 1.0f);
}
