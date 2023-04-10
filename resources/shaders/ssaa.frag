#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout (binding = 0) uniform sampler2D image;

void main()
{
  vec2 coord = vOut.texCoord;
  vec4 r = textureGather(image, coord, 0);
  vec4 g = textureGather(image, coord, 1);
  vec4 b = textureGather(image, coord, 2);
  vec4 a = textureGather(image, coord, 3);
  out_fragColor.r = (r.x + r.y + r.z + r.w) / 4.;
  out_fragColor.g = (g.x + g.y + g.z + g.w) / 4.;
  out_fragColor.b = (b.x + b.y + b.z + b.w) / 4.;
  out_fragColor.a = (a.x + a.y + a.z + a.w) / 4.;
}
