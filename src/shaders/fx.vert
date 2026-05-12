#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct InstanceData {
  vec3 pos;
  float t;
  float max_t;
};

layout(buffer_reference, scalar) readonly buffer InstanceBuffer {
  InstanceData instances[];
};

layout(push_constant) uniform Push {
  mat4 view;
  mat4 proj;
  InstanceBuffer instance_buffer;
} pc;

vec3 verts[6] = vec3[6](
    vec3(-1, -1, 0),
    vec3(1, 1, 0),
    vec3(1, -1, 0),
    vec3(-1, 1, 0),
    vec3(1, 1, 0),
    vec3(-1, -1, 0)
  );

layout(location = 0) out vec3 fragColor;
layout(location = 1) out float t;

void main() {
  InstanceData instance = pc.instance_buffer.instances[gl_InstanceIndex];
  gl_Position = (
    (pc.proj
      * ((pc.view * vec4(instance.pos, 1))
      + vec4(verts[gl_VertexIndex] * vec3(5, 5, 5), 0))))
      * vec4(1., -1., 1., 1.);
  fragColor = vec3(1., 1., 1);
  t = 1. - (instance.t / instance.max_t);
}
