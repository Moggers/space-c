#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference_uvec2 : enable

struct UiVertex {
  vec2 pos;
  vec2 uv;
  uint col;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
  UiVertex vertices[];
};

layout(push_constant) uniform Push {
  vec2 viewport;
  VertexBuffer vertex_buffer;
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragCol;

void main() {
  UiVertex v = pc.vertex_buffer.vertices[gl_VertexIndex];
  vec2 ndc = (v.pos / pc.viewport) * 2.0 - 1.0;
  gl_Position = vec4(ndc, 0.0, 1.0);
  fragUV = v.uv;
  fragCol = vec4(
    float((v.col >>  0) & 0xFFu),
    float((v.col >>  8) & 0xFFu),
    float((v.col >> 16) & 0xFFu),
    float((v.col >> 24) & 0xFFu)
  ) / 255.0;
}
