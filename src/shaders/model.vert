#version 460
#extension GL_EXT_buffer_reference : require
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
  mat4 transform;
  VertexBuffer vertex_buffer;
  vec3 col;
  vec3 scale;
  float dead_time;
};

layout(buffer_reference, scalar) readonly buffer InstanceBuffer {
  InstanceData instances[];
};

layout(push_constant) uniform Push {
  mat4 view;
  mat4 proj;
  InstanceBuffer instance_buffer;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
  InstanceData instance = pc.instance_buffer.instances[gl_InstanceIndex];
  gl_Position = ((pc.proj * pc.view) * instance.transform * vec4((instance.vertex_buffer.vertices[gl_VertexIndex].pos + (instance.vertex_buffer.vertices[gl_VertexIndex].normal * instance.dead_time)) * instance.scale, 1.0)) * vec4(1., -1., 1., 1.);
  fragColor = instance.vertex_buffer.vertices[gl_VertexIndex].col
      * instance.col
      * max(0.01, dot(
          vec3(mat3(instance.transform) * instance.vertex_buffer.vertices[gl_VertexIndex].normal),
          normalize(vec3(1., 1., 0.))
        ));
}
