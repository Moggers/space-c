#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
  vec3 pos;
  vec3 normal;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
  Vertex vertices[];
};

layout(push_constant) uniform Push {
  mat4 camera;
  VertexBuffer vtx; // these are 64-bit "pointers"
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
  gl_Position = pc.camera * vec4(pc.vtx.vertices[gl_VertexIndex].pos, 1.0);
  fragColor = vec3(1.,1.,1.) * dot(pc.vtx.vertices[gl_VertexIndex].normal, vec3(0., 1., 0.));
}
