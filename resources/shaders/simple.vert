#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "common.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;

} vOut;


layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};


mat4 rotateY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return transpose(mat4(
        c,   0.0, s,   0.0,
        0.0, 1.0, 0.0, 0.0,
       -s,   0.0, c,   0.0,
        0.0, 0.0, 0.0, 1.0
    ));
}

mat4 translate(vec3 t)
{
    return transpose(mat4(
        1.0, 0.0, 0.0, t.x,
        0.0, 1.0, 0.0, t.y,
        0.0, 0.0, 1.0, t.z,
        0.0, 0.0, 0.0, 1.0
    ));
}

mat4 teapotModel = transpose(mat4(
    0.999889,  0.0, -0.0149171, 0.0,
    0.0,       1.0,  0,        -1.27,
    0.0149171, 0.0,  0.999889,  0.0,
    0.0,       0.0,  0.0,       1.0
));

mat4 sphereModel = transpose(mat4(
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.75,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
));

mat4 LModel = transpose(mat4(
    0.824126, 0.0, 0.566406, -0.0100791,
    0.0,      1.0, 0.0,      -1.27,
   -0.566406, 0.0, 0.824126,  0.872508,
    0.0,      0.0, 0.0,       1.0
));

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);
    mat4 mModel = params.mModel;
    if (mModel == teapotModel)
    {
        mModel *= rotateY(Params.time);
    }
    if (mModel == sphereModel)
    {
        vec3 t = vec3(0.0, 0.0, 1.0);
        mModel *= translate(t*(1.0+sin(Params.time)));
    }
    if (mModel == LModel)
    {
        mModel *= rotateY(0.5*(sin(Params.time)-1.0));
    }
    vOut.wPos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(mModel))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(mModel))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}
