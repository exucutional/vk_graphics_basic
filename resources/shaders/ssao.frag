#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out float out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vsOut;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D positionMap;
layout (binding = 2) uniform sampler2D normalMap;
layout (binding = 3) buffer ssaoSampleBuf 
{
    vec4 ssaoSamples[];
};
layout (binding = 4) buffer ssaoNoiseBuf 
{
    vec4 ssaoNoiseVector[];
};

void main()
{
  vec3 vPos = texture(positionMap, vsOut.texCoord).xyz;
  vec2 noiseCoord = mod(vec2(vsOut.texCoord.x * Params.width, vsOut.texCoord.y * Params.height), Params.ssaoNoiseSize - 1);
  vec3 noiseVector = ssaoNoiseVector[uint(noiseCoord.x * Params.ssaoNoiseSize + noiseCoord.y)].xyz;
  vec3 n   = texture(normalMap, vsOut.texCoord).xyz;
  vec3 t   = normalize(noiseVector - n * dot(noiseVector, n));
  vec3 b   = cross(n, t);
  mat3 tbn = mat3(t, b, n);
  float occlusion = 0.0;
  for (int i = 0; i < Params.ssaoSampleSize; ++i)
  {
    vec3 ssaoSample = tbn * ssaoSamples[i].xyz;
    vec3 sampleVPos = vPos + ssaoSample * Params.ssaoRadius;
    vec4 sampleCPos = Params.proj * vec4(sampleVPos, 1.0);
    vec3 sampleNBCPos = sampleCPos.xyz / sampleCPos.w;
    vec3 samplePos = sampleNBCPos * 0.5 + 0.5;
    float sampleDepth = texture(positionMap, samplePos.xy).z;
    float rangeCheck = smoothstep(0.0, 1.0, Params.ssaoRadius / abs(vPos.z - sampleDepth));
    occlusion += (sampleDepth >= sampleVPos.z ? 1.0 : 0.0) * rangeCheck;
  }
  occlusion = 1.0 - (occlusion / Params.ssaoSampleSize);
  out_fragColor = occlusion;  
}
