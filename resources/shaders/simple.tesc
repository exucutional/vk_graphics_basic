#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(vertices = 4) out;

layout (location = 0) in TESC_IN
{
    vec3 wPos;
    vec2 texCoord;
} tescIn[];

layout (location = 0 ) out TESC_OUT
{
    vec3 wPos;
    vec2 texCoord;
} tescOut[];

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

void main()
{
    tescOut[gl_InvocationID].wPos = tescIn[gl_InvocationID].wPos;
    tescOut[gl_InvocationID].texCoord = tescIn[gl_InvocationID].texCoord;

    gl_TessLevelOuter[0] = Params.tessLevel;
    gl_TessLevelOuter[1] = Params.tessLevel;
    gl_TessLevelOuter[2] = Params.tessLevel;
    gl_TessLevelOuter[3] = Params.tessLevel;

    gl_TessLevelInner[0] = Params.tessLevel;
    gl_TessLevelInner[1] = Params.tessLevel;
}
