#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"


layout(location = 0) in vec4 vPos;
layout(location = 1) in vec4 vTexCoord;

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec2 texCoord;
} vOut;

void main(void)
{
    vOut.wPos     = vPos.xyz;
    vOut.texCoord = vTexCoord.xy;
}
