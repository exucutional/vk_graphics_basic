#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(push_constant) uniform params_t
{
  vec4 scaleAndOffs;
  mat4 mProjInverse;
  mat4 mViewInverse;
  uint TileDim;
  uint LightCount;
  float LightRadius;
} PushConstant;

layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform sampler2D depthMap;
layout (binding = 3) uniform sampler2D gNormalMap;

layout(binding = 4) readonly buffer LightPosB
{
    vec3 LightPos[];
};

layout(binding = 5) readonly buffer LightColorB
{
    vec4 LightColor[];
};

layout(binding = 6) readonly buffer TileLightIndexesB
{
    uint TileLightIndexes[];    //2d [TileCount][LightCount]
};

layout(binding = 7) readonly buffer TileLightCountB
{
    uint TileLightCount[];
};

vec3 restore_world_position_from_depth()
{
  float depth       = texture(depthMap, surf.texCoord).x;
  float x           = surf.texCoord.x*2.0f-1.0f;
  float y           = (1.0f-surf.texCoord.y)*2.0f-1.0f;
  vec4 vPosUnScaled = PushConstant.mProjInverse*vec4(x, y, depth, 1.0f);
  vec3 vPos         = 2.0f*vPosUnScaled.xyz/vPosUnScaled.w;
  vec4 wPos         = PushConstant.mViewInverse*vec4(vPos, 1.0f); 
  return wPos.xyz;
}

void main()
{
  vec3 wPos  = restore_world_position_from_depth();
  vec3 wNorm = texture(gNormalMap, surf.texCoord).rgb;
  vec4 sumLightColor = vec4(0.0f);
  uint tilex  = uint(floor(surf.texCoord.x*PushConstant.TileDim));
  uint tiley  = uint(floor((1-surf.texCoord.y)*PushConstant.TileDim));
  uint tileId = tiley*PushConstant.TileDim+tilex;
  for (int i = 0; i < TileLightCount[tileId]; i++)
  {
    uint lightId    = TileLightIndexes[tileId*PushConstant.LightCount+i];
    vec3 lightPos   = LightPos[lightId];
    float dist      = distance(lightPos, wPos);
    float att       = clamp(1.0-dist*dist/(PushConstant.LightRadius*PushConstant.LightRadius), 0.0, 1.0);
    vec4 color      = LightColor[lightId];
    vec3 lightDir   = normalize(lightPos-wPos);
    vec4 lightColor = max(dot(wNorm, lightDir), 0.0f)*color;
    sumLightColor   += att*lightColor;
  }
  out_fragColor= (sumLightColor+vec4(0.1f))*vec4(Params.baseColor, 1.0f);
}
