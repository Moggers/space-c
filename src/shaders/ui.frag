#version 460

layout(set = 0, binding = 0) uniform sampler2D fontTex;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragCol;

layout(location = 0) out vec4 outCol;

void main() {
  outCol = fragCol * texture(fontTex, fragUV);
}
