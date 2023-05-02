#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint albedoId;
} params;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform sampler2D rsmPosition;
layout (binding = 3) uniform sampler2D rsmNormal;
layout (binding = 4) uniform sampler2D rsmFlux;
layout (binding = 5) buffer rsmSamplesBuf
{
  vec2 rsmSamples[];
};

vec4 accumulateFlux(vec3 wPos, vec3 wNorm, vec2 texCoord)
{
  vec4 acc = vec4(0.0f);
  for (int i = 0; i < Params.rsmSampleCount; ++i)
  {
    const vec2 randomCoord = texCoord + rsmSamples[i]*Params.rsmRadius;
    const vec3 sampleWPos = texture(rsmPosition, randomCoord).xyz;
    const vec3 sampleNormal = texture(rsmNormal, randomCoord).xyz;
    const vec4 sampleFlux = texture(rsmFlux, randomCoord);

    acc += sampleFlux
      *max(0, dot(sampleNormal, wPos-sampleWPos))
      *max(0, dot(wNorm, sampleWPos-wPos))
      /pow(length(wPos-sampleWPos), 4);
  }
  return clamp(acc * Params.rsmIntentsity, 0.0f, 1.0f);
}

void main()
{
  const vec4 posLightClipSpace = Params.lightMatrix*vec4(surf.wPos, 1.0f); // 
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]               
    
  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec4 GI = accumulateFlux(surf.wPos, surf.wNorm, shadowTexCoord);

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(Params.time)));
  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
   
  vec3 lightDir   = normalize(Params.lightPos - surf.wPos);
  vec4 lightColor = max(dot(surf.wNorm, lightDir), 0.0f) * lightColor2;
  out_fragColor = vec4(getAlbedo(params.albedoId), 1.0f)*lightColor*shadow;
  if (Params.rsmEnabled)
    out_fragColor += GI;
}
