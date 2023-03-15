#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout (quads, equal_spacing, cw) in;

layout (push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint noiseSize;
} pushConstant;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D shadowMap;

layout (binding = 2) buffer noiseMapB
{
    float noiseMap[];
};

layout (location = 0) in TESE_IN
{
    vec3 wPos;
    vec2 texCoord;
} teseIn[];

layout (location = 0) out TESE_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec2 texCoord;
} teseOut;

vec2 interpolateVec2(float x, float y, vec2 tc0, vec2 tc1, vec2 tc2, vec2 tc3)
{
    vec2 t0 = (tc1 - tc0) * x + tc0;
    vec2 t1 = (tc3 - tc2) * x + tc2;
    return (t1 - t0) * y + t0;
}

vec3 interpolateVec3(float x, float y, vec3 tc0, vec3 tc1, vec3 tc2, vec3 tc3)
{
    vec3 t0 = (tc1 - tc0) * x + tc0;
    vec3 t1 = (tc3 - tc2) * x + tc2;
    return (t1 - t0) * y + t0;
}

void main()
{
    vec2 iTexCoord = interpolateVec2(
        gl_TessCoord.x, gl_TessCoord.y, teseIn[0].texCoord,
        teseIn[1].texCoord, teseIn[2].texCoord, teseIn[3].texCoord);

    vec3 iWPos = interpolateVec3(
        gl_TessCoord.x, gl_TessCoord.y, teseIn[0].wPos,
        teseIn[1].wPos, teseIn[2].wPos, teseIn[3].wPos);

    uvec2 noiseCoord = uvec2(iTexCoord * (pushConstant.noiseSize - 1));
    iWPos.y += noiseMap[noiseCoord.x * pushConstant.noiseSize + noiseCoord.y];

    vec3 surfaceU = teseIn[1].wPos - teseIn[0].wPos;
    vec3 surfaceV = teseIn[2].wPos - teseIn[0].wPos;
    vec4 surfaceNormal = normalize(-vec4(cross(surfaceU, surfaceV), 0));

    teseOut.wPos = iWPos;
    teseOut.wNorm = surfaceNormal.xyz;
    teseOut.texCoord = iTexCoord;

    gl_Position = pushConstant.mProjView * vec4(iWPos, 1.0);
}
