#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct InstanceData {
  vec3 pos;
  float t;
};

layout(buffer_reference, scalar) readonly buffer InstanceBuffer {
  InstanceData instances[];
};

layout(push_constant) uniform Push {
  mat4 camera;
  InstanceBuffer instance_buffer;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
  gl_Position = vec4(1., 1., 1., 1.);
  fragColor = vec3(1., 1., 1.);
}
