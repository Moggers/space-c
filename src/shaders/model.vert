#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
  vec3 pos;
  vec3 normal;
  vec3 col;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer {
  Vertex vertices[];
};

struct InstanceData {
  VertexBuffer vertex_buffer;
  vec3 pos;
  mat4 rot;
};

layout(buffer_reference, scalar) readonly buffer InstanceBuffer{
  InstanceData instances[];
};

layout(push_constant) uniform Push {
  mat4 camera;
  InstanceBuffer instance_buffer;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
  gl_Position = pc.camera * vec4(pc.instance_buffer.instances[gl_DrawID].vertex_buffer.vertices[gl_VertexIndex].pos, 1.0);
  // fragColor = pc.vtx.vertices[gl_VertexIndex].col; //* dot(pc.vtx.vertices[gl_VertexIndex].normal, vec3(0., 1., 0.));
  fragColor = vec3(1., 1., 1.) * dot(pc.instance_buffer.instances[gl_DrawID].vertex_buffer.vertices[gl_VertexIndex].normal, vec3(0., 1., 0.));
}
