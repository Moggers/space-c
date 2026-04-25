#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
  vec3 vertices[];
};

layout(push_constant) uniform Push {
  mat4 camera;
  VertexBuffer vtx; // these are 64-bit "pointers"
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
  gl_Position = pc.camera * vec4(pc.vtx.vertices[gl_VertexIndex], 1.0);
  fragColor = vec3(1.0, 0.0, 0.0);
}
