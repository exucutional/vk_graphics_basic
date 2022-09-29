#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

//Bilateral filter
void main()
{
  vec4 colorC = textureLod(colorTex, surf.texCoord, 0);
  vec4 colorSum = vec4(0.);
  ivec2 textureSz = textureSize(colorTex, 0);
  float wSum = 0.;
  float r = 5.;
  float h = .15;
  for (float i = -r; i <= r; i++)
  {
    for (float j = -r; j <= r; j++)
    {
      vec4 colorNbr =  textureLod(colorTex, surf.texCoord+vec2(i, j)/textureSz, 0);
      float spaceDst = length(vec2(i, j));
      float spaceDst2 = spaceDst*spaceDst;
      float colorDst = length(colorC-colorNbr);
      float colorDst2 = colorDst*colorDst;
      float r2 = r*r;
      float h2 = h*h;
      float w = exp(-spaceDst2/r2-colorDst2/h2);
      colorSum += w*colorNbr;
      wSum += w;
    }
  }
  color = colorSum/wSum;
}
