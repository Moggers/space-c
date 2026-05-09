#ifndef VLK_HEADER
#define VLK_HEADER
#include "./vendor/cglm/cglm.h"
#include "./vendor/fast_obj.h"
#include "engine.h"
#include "vendor/cglm/mat4.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#define VK_WRAP(expr)                                                          \
  {                                                                            \
    VkResult res = expr;                                                       \
    if (res != VK_SUCCESS) {                                                   \
      fprintf(stderr, "Expected VK_SUCCESS, got %d\n", res);                   \
      assert(VK_SUCCESS == res);                                               \
    }                                                                          \
  }

#define VK_WRAP_ARR(type, name, fn, ...)                                       \
  uint32_t name##_count = 0;                                                   \
  fn(__VA_ARGS__, &name##_count, 0);                                           \
  type name[name##_count];                                                     \
  fn(__VA_ARGS__, &name##_count, name);

#define V3(v) {(v)[0], (v)[1], (v)[2]}
#define M4(m)                                                                  \
  {                                                                            \
      {(m)[0][0], (m)[0][1], (m)[0][2], (m)[0][3]},                            \
      {(m)[1][0], (m)[1][1], (m)[1][2], (m)[1][3]},                            \
      {(m)[2][0], (m)[2][1], (m)[2][2], (m)[2][3]},                            \
      {(m)[3][0], (m)[3][1], (m)[3][2], (m)[3][3]},                            \
  }

typedef struct Model {
  uint32_t vert_count;
  VkDeviceAddress device_addr;
} Model;

typedef struct SomeShitAllocated {
  void *the_shit_on_host;
  VkDeviceAddress the_shit_on_device;
  uint32_t offset_in_buffer;
} SomeShitAllocated;

typedef struct ModelInstanceData {
  float transform[16];
  VkDeviceAddress vertex_buffer;
  vec3 col;
  vec3 scale;
  float dead_time;
  uint32_t entity_id;
} ModelInstanceData;

typedef struct ModelInstanceDataBuffer {
  VkDeviceAddress bdaInstanceBuffer;
  ModelInstanceData *hostInsanceBuffer;
} ModelInstanceDataBuffer;

typedef struct FxInstanceData {
  vec3 pos;
  float t;
  float max_t;
} FxInstanceData;

#define MAX_DRAWS 1e6
#define MAX_FX_DRAWS 1e6

typedef struct DrawCommands {
  VkDeviceAddress bdaDrawCommands;
  VkDrawIndirectCommand *hostDrawCommands;
  VkDeviceSize bdaBufferOffset;
} DrawCommands;

typedef struct FxInstanceDataBuffer {
  VkDeviceAddress bdaInstanceBuffer;
  FxInstanceData *hostInsanceBuffer;
} FxInstanceDataBuffer;

typedef struct SelectionDataBuffer {
  VkDeviceAddress bdaSelectionData;
  uint32_t *hostSelectionData;
  VkDeviceSize begin;
} SelectionDataBuffer;

typedef struct SelectionPC {
  VkDeviceAddress outBuffer;
  int32_t image_size[2];
  uint32_t max_values;
} SelectionPC;

// Global vulkan shit
VkInstance GAME_VK_INSTANCE              = VK_NULL_HANDLE;
VkSurfaceKHR GAME_VK_SURFACE             = VK_NULL_HANDLE;
VkDevice GAME_VK_DEVICE                  = VK_NULL_HANDLE;
VkPhysicalDevice GAME_VK_PHYSICAL_DEVICE = VK_NULL_HANDLE;

// Swapchain shit
VkSwapchainKHR GAME_VK_SWAPCHAIN = VK_NULL_HANDLE;
VkImage GAME_SWAPCHAIN_IMAGES[32];
VkImageView GAME_SWAPCHAIN_IMAGE_VIEWS[32];
VkImage GAME_SWAPCHAIN_DEPTH_ATTACHMENTS[32];
VkImageView GAME_SWAPCHAIN_DEPTH_VIEWS[32];
uint32_t GAME_SWAPCHAIN_IMAGE_COUNT = 32;

// Depth buffer
VkDeviceMemory GAME_VK_DEPTH_MEMORY;
VkImage GAME_VK_DEPTH_IMAGE;
VkImageView GAME_VK_DEPTH_IMAGE_VIEW;

// Queues and command buffers
VkQueue GAME_VK_PRESENT_QUEUE          = VK_NULL_HANDLE;
VkCommandPool GAME_VK_COMMAND_POOL     = VK_NULL_HANDLE;
VkCommandBuffer GAME_VK_COMMAND_BUFFER = VK_NULL_HANDLE;

// Graphics pipeline shit
VkPipeline GAME_VK_MODEL_PIPELINE        = VK_NULL_HANDLE;
VkPipeline GAME_VK_FX_PIPELINE           = VK_NULL_HANDLE;
VkPipelineLayout GAME_VK_PIPELINE_LAYOUT = VK_NULL_HANDLE;
VkShaderModule GAME_VERT_MODULE          = VK_NULL_HANDLE;
VkShaderModule GAME_FRAG_MODULE          = VK_NULL_HANDLE;
VkSurfaceFormatKHR GAME_VK_SURFACE_FORMAT;
VkSurfaceCapabilitiesKHR GAME_SURFACE_CAPABILITIES;
VkFence GAME_PRESENT_FENCE;

// Assets
Model GAME_MODELS[512];
uint32_t GAME_MODEL_COUNT = 1;

// Render state
uint32_t GAME_CURRENT_SWAPCHAIN_IMAGE = 0;
DrawCommands GAME_MODEL_DRAW_COMMANDS;
DrawCommands GAME_FX_DRAW_COMMANDS;
ModelInstanceDataBuffer GAME_MODEL_INSTANCE_BUFFER;
FxInstanceDataBuffer GAME_FX_INSTANCE_BUFFER;
;

// Camera shit
mat4 GAME_CAM_TRANSFORM;
mat4 GAME_PERSP_PROJ;

// Arena
VkBuffer GAME_VK_ALL_THE_DATA;
void *GAME_VK_ALL_THE_DATA_HOST;
size_t ALL_THE_DATA_HEAD = 0;

void vlk_createShaderModule(char *path, VkShaderModule *module) {
  FILE *f = fopen(path, "rb");
  assert(f != 0);
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  char shader_src[length];
  fseek(f, 0, SEEK_SET);
  fread(shader_src, length, 1, f);
  fclose(f);
  vkCreateShaderModule(GAME_VK_DEVICE,
                       &(VkShaderModuleCreateInfo){
                           .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                           .codeSize = length,
                           .pCode    = (void *)shader_src},
                       0, module);
}

typedef struct {
  mat4 view;
  mat4 proj;
  VkDeviceAddress instance_buffer;
} PushConstant;

void vlk_allocateSomeShit(size_t size, SomeShitAllocated *out_buff) {
  VkDeviceAddress addr = vkGetBufferDeviceAddress(
      GAME_VK_DEVICE,
      &(VkBufferDeviceAddressInfo){
          .buffer = GAME_VK_ALL_THE_DATA,
          .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO});

  out_buff->the_shit_on_device = addr + ALL_THE_DATA_HEAD;
  out_buff->the_shit_on_host   = GAME_VK_ALL_THE_DATA_HOST + ALL_THE_DATA_HEAD;
  out_buff->offset_in_buffer   = ALL_THE_DATA_HEAD;
  ALL_THE_DATA_HEAD += size;

  return;
}

typedef struct Vertex {
  vec3 pos;
  vec3 norm;
  vec3 col;
} Vertex;

vec3 debug_cols[4] = {
    {1., 0., 0.},
    {0., 1., 0.},
    {0., 0., 1.},
    {0., 1., 1.},
};

unsigned int obj_to_indexbuffer(fastObjMesh *brush,
                                SomeShitAllocated *vertex_buffer) {

  // First calculate the number of vertices
  unsigned int out_index_count = 0;

  // For each group in the obj
  for (int group_n = 0; group_n < brush->group_count; group_n++) {
    fastObjGroup grp = brush->groups[group_n];

    // For each face in the group
    for (int face_n = 0; face_n < grp.face_count; face_n++) {

      // Fan out; triangle = one tri (3 = 3), quad = two tries (6 = 4), 5-gon =
      // three tris (9 = 5) So Face tri's index count = (face n-gon's index
      // count - 2) * 3
      out_index_count +=
          (brush->face_vertices[grp.face_offset + face_n] - 2) * 3;
    }
  }

  // Position and normal
  vlk_allocateSomeShit(out_index_count * sizeof(Vertex), vertex_buffer);
  Vertex *vertex_buffer_write = (Vertex *)(vertex_buffer->the_shit_on_host);

  // Now populate the verts
  uint32_t out_idx = 0;

  // For each group in the obj
  for (int group_n = 0; group_n < brush->group_count; group_n++) {
    fastObjGroup grp = brush->groups[group_n];

    unsigned int idx = grp.index_offset;

    // For each face in the group
    for (int face_n = 0; face_n < grp.face_count; face_n++) {
      // Get the count of vertices in the face
      unsigned int vert_count = brush->face_vertices[grp.face_offset + face_n];

      // Note down the start of the face in the obj's index array. We'll need it
      // below to fan out the face for triangulation.
      unsigned int face_start = idx;
      // If face is a tri, iterate once. If face is a quad, iterate twice, etc.
      for (int tri_n = 0; tri_n <= vert_count - 3; tri_n++) {

        // First index (of the tri) is always the first index of the face
        float *pos  = &brush->positions[brush->indices[face_start].p * 3];
        float *norm = &brush->normals[brush->indices[face_start].n * 3];
        // float *col  = debug_cols[face_n & 0b11];
        float *col = (vec3){1., 1., 1.};
        vertex_buffer_write[out_idx++] =
            (Vertex){.pos = V3(pos), .norm = V3(norm), .col = V3(col)};

        // Other two points, step through
        pos  = &brush->positions[brush->indices[idx + 1].p * 3];
        norm = &brush->normals[brush->indices[idx + 1].n * 3];
        vertex_buffer_write[out_idx++] =
            (Vertex){.pos = V3(pos), .norm = V3(norm), .col = V3(col)};
        pos  = &brush->positions[brush->indices[idx + 2].p * 3];
        norm = &brush->normals[brush->indices[idx + 2].n * 3];
        vertex_buffer_write[out_idx++] =
            (Vertex){.pos = V3(pos), .norm = V3(norm), .col = V3(col)};
        // Index (0,1,2),(0,2,3), (0,3,4) for a 5-gon

        // The index array stores faces contiguously, we are *actually*
        // looping the entire index array a single time after we loop over
        // each face.
        idx++;
      }
      // Because we loop as many times as we have vertices on the face - 2; we
      // skip two ahead.
      idx += 2;
    }
  }

  return out_index_count;
}

unsigned int load_model(fastObjMesh *brush) {
  assert(brush != 0);
  SomeShitAllocated vertex_buffer;
  unsigned int vertex_count = obj_to_indexbuffer(brush, &vertex_buffer);

  GAME_MODELS[GAME_MODEL_COUNT] =
      (Model){.vert_count  = vertex_count,
              .device_addr = vertex_buffer.the_shit_on_device};
  return GAME_MODEL_COUNT++;
}

void vlk_CreateDepthBuffer() {
  if (GAME_VK_DEPTH_IMAGE_VIEW != VK_NULL_HANDLE) {
    vkDestroyImageView(GAME_VK_DEVICE, GAME_VK_DEPTH_IMAGE_VIEW, 0);
    GAME_VK_DEPTH_IMAGE_VIEW = VK_NULL_HANDLE;
  }
  if (GAME_VK_DEPTH_IMAGE != VK_NULL_HANDLE) {
    vkDestroyImage(GAME_VK_DEVICE, GAME_VK_DEPTH_IMAGE, 0);
    GAME_VK_DEPTH_IMAGE = VK_NULL_HANDLE;
  }
  if (GAME_VK_DEPTH_MEMORY != VK_NULL_HANDLE) {
    vkFreeMemory(GAME_VK_DEVICE, GAME_VK_DEPTH_MEMORY, 0);
    GAME_VK_DEPTH_MEMORY = VK_NULL_HANDLE;
  }

  vkCreateImage(
      GAME_VK_DEVICE,
      &(VkImageCreateInfo){
          .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType   = VK_IMAGE_TYPE_2D,
          .arrayLayers = 1,
          .extent =
              (VkExtent3D){
                  .width  = GAME_SURFACE_CAPABILITIES.currentExtent.width,
                  .height = GAME_SURFACE_CAPABILITIES.currentExtent.height,
                  .depth  = 1},
          .samples   = 1,
          .format    = VK_FORMAT_D32_SFLOAT,
          .usage     = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          .mipLevels = 1,
      },
      0, &GAME_VK_DEPTH_IMAGE);
  VkMemoryRequirements memreqs;
  VkMemoryPropertyFlagBits wanted = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  vkGetImageMemoryRequirements(GAME_VK_DEVICE, GAME_VK_DEPTH_IMAGE, &memreqs);
  VkPhysicalDeviceMemoryProperties memprops;
  vkGetPhysicalDeviceMemoryProperties(GAME_VK_PHYSICAL_DEVICE, &memprops);
  int memtype = -1;
  for (int i = 0; i < memprops.memoryTypeCount; i++) {
    // If this memory type is not one of the types usable by the buffer; skip
    // it
    if ((memreqs.memoryTypeBits & (1 << i)) == 0) {
      continue;
    }

    // If the memory type does not have the flags we want, skip it.
    if ((memprops.memoryTypes[i].propertyFlags & wanted) != wanted) {
      continue;
    }
    // We found one we can use
    memtype = i;
    break;
  }
  assert(memtype != -1);
  vkAllocateMemory(
      GAME_VK_DEVICE,
      &(VkMemoryAllocateInfo){.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                              .allocationSize  = memreqs.size,
                              .memoryTypeIndex = memtype},
      0, &GAME_VK_DEPTH_MEMORY);
  vkBindImageMemory(GAME_VK_DEVICE, GAME_VK_DEPTH_IMAGE, GAME_VK_DEPTH_MEMORY,
                    0);

  vkCreateImageView(
      GAME_VK_DEVICE,
      &(VkImageViewCreateInfo){
          .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image    = GAME_VK_DEPTH_IMAGE,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format   = VK_FORMAT_D32_SFLOAT,
          .subresourceRange =
              (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                        .baseArrayLayer = 0,
                                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                                        .baseMipLevel = 0,
                                        .levelCount = VK_REMAINING_MIP_LEVELS}},
      0, &GAME_VK_DEPTH_IMAGE_VIEW);
}

// Returns 1 on failure
VkResult vlk_createSwapchain() {
  VkResult res;
  VkSwapchainKHR old = GAME_VK_SWAPCHAIN;

  VK_WRAP_ARR(VkSurfaceFormatKHR, formats, vkGetPhysicalDeviceSurfaceFormatsKHR,
              GAME_VK_PHYSICAL_DEVICE, GAME_VK_SURFACE);
  for (int i = 0; i < formats_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
      GAME_VK_SURFACE_FORMAT = formats[i];
      break;
    }
  }
  if (GAME_VK_SURFACE_FORMAT.format == 0) {
    fprintf(stderr, "Cant get format");
    exit(1);
  }
  res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      GAME_VK_PHYSICAL_DEVICE, GAME_VK_SURFACE, &GAME_SURFACE_CAPABILITIES);
  printf("Current extent %d,%d\n",
         GAME_SURFACE_CAPABILITIES.currentExtent.width,
         GAME_SURFACE_CAPABILITIES.currentExtent.height);
  if (res != VK_SUCCESS)
    return res;
  res = vkCreateSwapchainKHR(
      GAME_VK_DEVICE,
      &(VkSwapchainCreateInfoKHR){
          .oldSwapchain     = GAME_VK_SWAPCHAIN,
          .presentMode      = VK_PRESENT_MODE_IMMEDIATE_KHR,
          .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
          .surface          = GAME_VK_SURFACE,
          .minImageCount    = GAME_SURFACE_CAPABILITIES.minImageCount + 1,
          .imageFormat      = GAME_VK_SURFACE_FORMAT.format,
          .imageColorSpace  = GAME_VK_SURFACE_FORMAT.colorSpace,
          .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
          .preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
          .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
          .imageArrayLayers = 1,
          .imageExtent      = GAME_SURFACE_CAPABILITIES.currentExtent},
      0, &GAME_VK_SWAPCHAIN);
  if (res != VK_SUCCESS) {
    GAME_VK_SWAPCHAIN = VK_NULL_HANDLE;
    printf("Failed to create swapchain: %d\n", res);
    return res;
  }
  GAME_SWAPCHAIN_IMAGE_COUNT = 32;
  vkGetSwapchainImagesKHR(GAME_VK_DEVICE, GAME_VK_SWAPCHAIN,
                          &GAME_SWAPCHAIN_IMAGE_COUNT, GAME_SWAPCHAIN_IMAGES);
  for (int i = 0; i < GAME_SWAPCHAIN_IMAGE_COUNT; i++) {
    res =
        vkCreateImageView(GAME_VK_DEVICE,
                          &(VkImageViewCreateInfo){
                              .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                              .format = GAME_VK_SURFACE_FORMAT.format,
                              .subresourceRange =
                                  (VkImageSubresourceRange){
                                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                      .baseArrayLayer = 0,
                                      .layerCount   = VK_REMAINING_ARRAY_LAYERS,
                                      .baseMipLevel = 0,
                                      .levelCount   = VK_REMAINING_MIP_LEVELS},
                              .image    = GAME_SWAPCHAIN_IMAGES[i],
                              .viewType = VK_IMAGE_VIEW_TYPE_2D},
                          0, &GAME_SWAPCHAIN_IMAGE_VIEWS[i]);
    if (res != VK_SUCCESS)
      return res;
  }
  vlk_CreateDepthBuffer();
  if (old != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(GAME_VK_DEVICE);
    vkDestroySwapchainKHR(GAME_VK_DEVICE, old, 0);
  }

  glm_perspective_lh_zo(
      glm_rad(90),
      (float)GAME_SURFACE_CAPABILITIES.currentExtent.width /
          (float)GAME_SURFACE_CAPABILITIES.currentExtent.height,
      1.f, 1000000.f, GAME_PERSP_PROJ);

  return VK_SUCCESS;
}

void vlk_init(SDL_Window *window) {

  glm_mat4_identity(GAME_CAM_TRANSFORM);

  uint32_t ext_count;

  const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&ext_count);

  VK_WRAP(vkCreateInstance(
      &(VkInstanceCreateInfo){
          .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
          .pApplicationInfo =
              &(VkApplicationInfo){.apiVersion = VK_API_VERSION_1_4},
          .enabledExtensionCount   = ext_count,
          .ppEnabledExtensionNames = extensions},
      0, &GAME_VK_INSTANCE));

  if (!SDL_Vulkan_CreateSurface(window, GAME_VK_INSTANCE, 0,
                                &GAME_VK_SURFACE)) {
    printf("Couldnt make sur: %s\n", SDL_GetError());
  }
  // Device
  uint32_t device_count;
  VK_WRAP(vkEnumeratePhysicalDevices(GAME_VK_INSTANCE, &device_count, 0));
  VkPhysicalDevice pdevices[device_count];
  VK_WRAP(
      vkEnumeratePhysicalDevices(GAME_VK_INSTANCE, &device_count, pdevices));
  for (int i = 0; i < device_count; i++) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(pdevices[i], &properties);
    printf("Device %s\n", properties.deviceName);
  }
  GAME_VK_PHYSICAL_DEVICE = pdevices[0];
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(GAME_VK_PHYSICAL_DEVICE, &properties);
  printf("Picked %s\n", properties.deviceName);
  VK_WRAP(vkCreateDevice(
      pdevices[0],
      &(VkDeviceCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
          .pEnabledFeatures =
              &(VkPhysicalDeviceFeatures){.multiDrawIndirect = VK_TRUE,
                                          .independentBlend  = VK_TRUE},
          .pNext =
              &(VkPhysicalDeviceDynamicRenderingFeatures){
                  .sType =
                      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
                  .dynamicRendering = VK_TRUE,
                  .pNext =
                      &(VkPhysicalDeviceSynchronization2Features){
                          .sType =
                              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
                          .synchronization2 = VK_TRUE,
                          .pNext =
                              &(VkPhysicalDeviceBufferDeviceAddressFeatures){
                                  .sType =
                                      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
                                  .bufferDeviceAddress = VK_TRUE,
                                  .pNext =
                                      &(VkPhysicalDeviceScalarBlockLayoutFeatures){
                                          .sType =
                                              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
                                          .scalarBlockLayout = VK_TRUE}}}},
          .queueCreateInfoCount  = 1,
          .enabledExtensionCount = 8,
          .ppEnabledExtensionNames =
              (const char *[]){VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                               VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
                               VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                               VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
                               VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                               VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
                               VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
                               VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME},
          // VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME},
          .pQueueCreateInfos =
              &(const VkDeviceQueueCreateInfo){
                  .sType      = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                  .queueCount = 1,
                  .pQueuePriorities = &(float){1.f}}},
      0, &GAME_VK_DEVICE));

  // Creating the arena for all our bullshit
  vkCreateBuffer(
      GAME_VK_DEVICE,
      &(VkBufferCreateInfo){.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                     VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                     VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT |
                                     VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
                            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                            .size  = 256ull * 1e6,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE},
      0, &GAME_VK_ALL_THE_DATA);
  VkMemoryRequirements memreqs;
  uint32_t memtype;
  vkGetBufferMemoryRequirements(GAME_VK_DEVICE, GAME_VK_ALL_THE_DATA, &memreqs);
  VkMemoryPropertyFlagBits wanted = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  VkPhysicalDeviceMemoryProperties memprops;
  vkGetPhysicalDeviceMemoryProperties(GAME_VK_PHYSICAL_DEVICE, &memprops);
  for (int i = 0; i < memprops.memoryTypeCount; i++) {
    // If this memory type is not one of the types usable by the buffer;
    // skip it
    if ((memreqs.memoryTypeBits & (1 << i)) == 0) {
      continue;
    }

    // If the memory type does not have the flags we want, skip it.
    if ((memprops.memoryTypes[i].propertyFlags & wanted) != wanted) {
      continue;
    }
    // We found one we can use
    memtype = i;
    break;
  }
  VkDeviceMemory buffer_back;
  vkAllocateMemory(
      GAME_VK_DEVICE,
      &(VkMemoryAllocateInfo){
          .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .memoryTypeIndex = memtype,
          .allocationSize  = memreqs.size,
          .pNext =
              &(VkMemoryAllocateFlagsInfo){
                  .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                  .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}},
      0, &buffer_back);
  vkBindBufferMemory(GAME_VK_DEVICE, GAME_VK_ALL_THE_DATA, buffer_back, 0);
  vkMapMemory(GAME_VK_DEVICE, buffer_back, 0, VK_WHOLE_SIZE, 0,
              &GAME_VK_ALL_THE_DATA_HOST);

  // Pipeline requirements, command buffers and queues etc
  VK_WRAP_ARR(VkQueueFamilyProperties, queue_props,
              vkGetPhysicalDeviceQueueFamilyProperties,
              GAME_VK_PHYSICAL_DEVICE);
  int graphics_queue_idx;
  for (graphics_queue_idx = 0; graphics_queue_idx < queue_props_count;
       graphics_queue_idx++) {
    if (queue_props[graphics_queue_idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      break;
    }
  }
  vkGetDeviceQueue(GAME_VK_DEVICE, graphics_queue_idx, 0,
                   &GAME_VK_PRESENT_QUEUE);

  vkCreateCommandPool(
      GAME_VK_DEVICE,
      &(VkCommandPoolCreateInfo){
          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
      0, &GAME_VK_COMMAND_POOL);
  vkAllocateCommandBuffers(
      GAME_VK_DEVICE,
      &(VkCommandBufferAllocateInfo){
          .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool        = GAME_VK_COMMAND_POOL,
          .commandBufferCount = 1,
          .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY},
      &GAME_VK_COMMAND_BUFFER);
  VK_WRAP(vkCreateFence(GAME_VK_DEVICE,
                        &(VkFenceCreateInfo){
                            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                        },
                        0, &GAME_PRESENT_FENCE));

  SomeShitAllocated draw_cmds;
  SomeShitAllocated instance_data;
  // Instances will go here
  vlk_allocateSomeShit(sizeof(ModelInstanceData) * MAX_DRAWS, &instance_data);
  GAME_MODEL_INSTANCE_BUFFER.bdaInstanceBuffer =
      instance_data.the_shit_on_device;
  GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer = instance_data.the_shit_on_host;
  // FX will go here
  vlk_allocateSomeShit(sizeof(ModelInstanceData) * MAX_FX_DRAWS,
                       &instance_data);
  GAME_FX_INSTANCE_BUFFER.bdaInstanceBuffer = instance_data.the_shit_on_device;
  GAME_FX_INSTANCE_BUFFER.hostInsanceBuffer = instance_data.the_shit_on_host;
  // Model draw commands will go here
  vlk_allocateSomeShit(sizeof(VkDrawIndirectCommand) * MAX_DRAWS, &draw_cmds);
  GAME_MODEL_DRAW_COMMANDS.bdaDrawCommands  = draw_cmds.the_shit_on_device;
  GAME_MODEL_DRAW_COMMANDS.hostDrawCommands = draw_cmds.the_shit_on_host;
  GAME_MODEL_DRAW_COMMANDS.bdaBufferOffset  = draw_cmds.offset_in_buffer;
  // Fx draw commands will go here
  vlk_allocateSomeShit(sizeof(VkDrawIndirectCommand) * MAX_DRAWS, &draw_cmds);
  GAME_FX_DRAW_COMMANDS.bdaDrawCommands  = draw_cmds.the_shit_on_device;
  GAME_FX_DRAW_COMMANDS.hostDrawCommands = draw_cmds.the_shit_on_host;
  GAME_FX_DRAW_COMMANDS.bdaBufferOffset  = draw_cmds.offset_in_buffer;

  vkCreatePipelineLayout(
      GAME_VK_DEVICE,
      &(VkPipelineLayoutCreateInfo){
          .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .setLayoutCount = 0,
          .pushConstantRangeCount = 1,
          .pPushConstantRanges =
              &(VkPushConstantRange){.offset     = 0,
                                     .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                     .size       = sizeof(PushConstant)},
      },
      0, &GAME_VK_PIPELINE_LAYOUT);
}

void vlk_createGraphicsPipeline(char *vert_shader, char *frag_shader,
                                VkPipeline *p) {
  VkShaderModule vert_module;
  VkShaderModule frag_module;

  vlk_createShaderModule(vert_shader, &vert_module);
  vlk_createShaderModule(frag_shader, &frag_module);

  VK_WRAP(vkCreateGraphicsPipelines(
      GAME_VK_DEVICE, VK_NULL_HANDLE, 1,
      &(VkGraphicsPipelineCreateInfo){
          .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
          .pNext =
              &(VkPipelineRenderingCreateInfoKHR){
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                  .colorAttachmentCount = 1,
                  .pColorAttachmentFormats =
                      (VkFormat[1]){GAME_VK_SURFACE_FORMAT.format},
                  .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
              },
          .pRasterizationState =
              &(VkPipelineRasterizationStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                  .lineWidth = 1.,
                  .frontFace = VK_FRONT_FACE_CLOCKWISE,
                  .cullMode  = VK_CULL_MODE_BACK_BIT},
          .pDepthStencilState =
              &(VkPipelineDepthStencilStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                  .depthTestEnable       = VK_TRUE,
                  .depthWriteEnable      = VK_TRUE,
                  .stencilTestEnable     = VK_FALSE,
                  .stencilTestEnable     = VK_FALSE,
                  .depthCompareOp        = VK_COMPARE_OP_LESS,
                  .depthBoundsTestEnable = VK_FALSE},
          .pColorBlendState =
              &(VkPipelineColorBlendStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                  .attachmentCount = 1,
                  .pAttachments =
                      &(VkPipelineColorBlendAttachmentState){
                          .blendEnable         = VK_TRUE,
                          .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                          .dstColorBlendFactor =
                              VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                          .colorBlendOp        = VK_BLEND_OP_ADD,
                          .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                          .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                          .alphaBlendOp        = VK_BLEND_OP_ADD,
                          .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
                                                 VK_COLOR_COMPONENT_G_BIT |
                                                 VK_COLOR_COMPONENT_B_BIT |
                                                 VK_COLOR_COMPONENT_A_BIT}},
          .pDynamicState =
              &(VkPipelineDynamicStateCreateInfo){
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                  .dynamicStateCount = 2,
                  .pDynamicStates =
                      (VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT,
                                         VK_DYNAMIC_STATE_SCISSOR}},
          .pViewportState =
              &(VkPipelineViewportStateCreateInfo){
                  .scissorCount = 1,
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                  .viewportCount = 1,
              },
          .pMultisampleState =
              &(VkPipelineMultisampleStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                  .rasterizationSamples = 1},
          .stageCount = 2,
          .layout     = GAME_VK_PIPELINE_LAYOUT,
          .pInputAssemblyState =
              &(VkPipelineInputAssemblyStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
          .pVertexInputState =
              &(VkPipelineVertexInputStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
          .pStages =
              (VkPipelineShaderStageCreateInfo[2]){
                  {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                   .stage = VK_SHADER_STAGE_VERTEX_BIT,
                   .pName = "main",
                   .module = vert_module},
                  {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                   .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .pName = "main",
                   .module = frag_module}

              }},
      0, p));
}

void vlk_createPipelines() {
  vlk_createGraphicsPipeline("./shaders/model_vertex.spv",
                             "./shaders/model_fragment.spv",
                             &GAME_VK_MODEL_PIPELINE);
  vlk_createGraphicsPipeline("./shaders/fx_vertex.spv",
                             "./shaders/fx_fragment.spv", &GAME_VK_FX_PIPELINE);
}

int vlk_beginDraw() {
  if (GAME_VK_SWAPCHAIN == VK_NULL_HANDLE) {
    vlk_createSwapchain();
  }

  VkResult res = vkAcquireNextImageKHR(
      GAME_VK_DEVICE, GAME_VK_SWAPCHAIN, 10000000, VK_NULL_HANDLE,
      GAME_PRESENT_FENCE, &GAME_CURRENT_SWAPCHAIN_IMAGE);
  vkWaitForFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE, VK_TRUE, 10000000);
  vkResetFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE);
  if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    vlk_createSwapchain();
    return 1;
  }
  vkBeginCommandBuffer(GAME_VK_COMMAND_BUFFER,
                       &(VkCommandBufferBeginInfo){
                           .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                       });
  vkCmdPipelineBarrier2(
      GAME_VK_COMMAND_BUFFER,
      &(VkDependencyInfo){
          .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 2,
          .pImageMemoryBarriers    = (VkImageMemoryBarrier2[]){
              {.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
               .image     = GAME_SWAPCHAIN_IMAGES[GAME_CURRENT_SWAPCHAIN_IMAGE],
               .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
               .newLayout = VK_IMAGE_LAYOUT_GENERAL,
               .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
               .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               .subresourceRange =
                   (VkImageSubresourceRange){
                       .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                       .baseArrayLayer = 0,
                       .baseMipLevel   = 0,
                       .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                       .levelCount     = VK_REMAINING_MIP_LEVELS}},
              {.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
               .image         = GAME_VK_DEPTH_IMAGE,
               .oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED,
               .newLayout     = VK_IMAGE_LAYOUT_GENERAL,
               .srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
               .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               .subresourceRange = (VkImageSubresourceRange){
                   .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                   .baseArrayLayer = 0,
                   .baseMipLevel   = 0,
                   .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                   .levelCount     = VK_REMAINING_MIP_LEVELS}}}});
  vkCmdSetViewport(
      GAME_VK_COMMAND_BUFFER, 0, 1,

      &(VkViewport){.height   = GAME_SURFACE_CAPABILITIES.currentExtent.height,
                    .width    = GAME_SURFACE_CAPABILITIES.currentExtent.width,
                    .maxDepth = 1.,
                    .minDepth = 0.,
                    .x        = 0,
                    .y        = 0});
  vkCmdSetScissor(GAME_VK_COMMAND_BUFFER, 0, 1,
                  &(VkRect2D){.extent = GAME_SURFACE_CAPABILITIES.currentExtent,
                              .offset = (VkOffset2D){.x = 0, .y = 0}});
  vkCmdBeginRendering(
      GAME_VK_COMMAND_BUFFER,
      &(VkRenderingInfo){
          .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .pDepthAttachment =
              &(VkRenderingAttachmentInfo){
                  .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                  .clearValue  = (VkClearValue){.depthStencil =
                                                    (VkClearDepthStencilValue){
                                                        .depth = 1.f}},
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                  .imageView   = GAME_VK_DEPTH_IMAGE_VIEW,
                  .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                  .storeOp     = VK_ATTACHMENT_STORE_OP_STORE},
          .colorAttachmentCount = 1,
          .layerCount           = 1,
          .pColorAttachments =
              (VkRenderingAttachmentInfo[1]){
                  {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                   .clearValue =
                       (VkClearValue){.color =
                                          (VkClearColorValue){
                                              .float32 = {0.0, 0.0, 0.0, 1.}}},
                   .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                   .imageView =
                       GAME_SWAPCHAIN_IMAGE_VIEWS[GAME_CURRENT_SWAPCHAIN_IMAGE],
                   .loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE}},
          .renderArea =
              (VkRect2D){.extent = GAME_SURFACE_CAPABILITIES.currentExtent,
                         .offset = (VkOffset2D){.x = 0, .y = 0}}});

  return 0;
}

void vlk_queueFxDrawCommands(uint32_t fx_count, vec3 *fx_loc, float *fx_t,
                             float *fx_max_t) {
  int drawn_fx_count = 0;
  for (uint32_t i = 0; i < fx_count; i++) {
    if (fx_t[i] > fx_max_t[i]) {
      continue;
    }
    glm_vec3_copy(
        fx_loc[i],
        GAME_FX_INSTANCE_BUFFER.hostInsanceBuffer[drawn_fx_count].pos);
    GAME_FX_INSTANCE_BUFFER.hostInsanceBuffer[drawn_fx_count].t = fx_t[i];
    GAME_FX_INSTANCE_BUFFER.hostInsanceBuffer[drawn_fx_count].max_t =
        fx_max_t[i];
    drawn_fx_count++;
  }
  // Add a draw command for this model; this may have an instance count of 0.
  GAME_FX_DRAW_COMMANDS.hostDrawCommands[0] =
      (VkDrawIndirectCommand){.firstInstance = 0,
                              .firstVertex   = 0,
                              .instanceCount = drawn_fx_count,
                              .vertexCount   = 6};
}

void vlk_queueModelDrawCommands(uint32_t entity_count, mat4 *entity_transforms,
                                vec3 *col, vec3 *scale, int32_t *entity_models,
                                float *dead_time) {
  // Start with zero instances
  uint32_t instance_count = 0;

  // For each model
  for (uint32_t model_index = 1; model_index < GAME_MODEL_COUNT;
       model_index++) {
    uint32_t first_instance = instance_count;

    // For each entity
    for (uint32_t i = 0; i < entity_count; i++) {

      // If the entity is the current model
      if (entity_models[i] == model_index) {
        // Push it to the instance buffer
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count]
            .vertex_buffer = GAME_MODELS[model_index].device_addr;
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].col[0] =
            col[i][0];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].col[1] =
            col[i][1];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].col[2] =
            col[i][2];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].scale[0] =
            scale[i][0];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].scale[1] =
            scale[i][1];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].scale[2] =
            scale[i][2];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].dead_time =
            dead_time[i];
        GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count].entity_id =
            i;
        glm_mat4_ucopy(
            (vec4 *)entity_transforms[i],
            (vec4 *)GAME_MODEL_INSTANCE_BUFFER.hostInsanceBuffer[instance_count]
                .transform);

        // Increment instance count
        instance_count++;
      }
    }

    // Add a draw command for this model; this may have an instance count of
    // 0.
    GAME_MODEL_DRAW_COMMANDS.hostDrawCommands[model_index] =
        (VkDrawIndirectCommand){
            .firstInstance = first_instance,
            .firstVertex   = 0,
            .instanceCount = instance_count - first_instance,
            .vertexCount   = GAME_MODELS[model_index].vert_count};
  }
}

void vlk_issueDraws() {
  // Cam matrix
  PushConstant pc = {.proj = M4(GAME_PERSP_PROJ)};
  glm_mat4_inv(GAME_CAM_TRANSFORM, pc.view);

  // Models
  pc.instance_buffer = GAME_MODEL_INSTANCE_BUFFER.bdaInstanceBuffer;
  vkCmdBindPipeline(GAME_VK_COMMAND_BUFFER, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GAME_VK_MODEL_PIPELINE);
  vkCmdPushConstants(GAME_VK_COMMAND_BUFFER, GAME_VK_PIPELINE_LAYOUT,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &pc);
  vkCmdDrawIndirect(GAME_VK_COMMAND_BUFFER, GAME_VK_ALL_THE_DATA,
                    GAME_MODEL_DRAW_COMMANDS.bdaBufferOffset, GAME_MODEL_COUNT,
                    sizeof(VkDrawIndirectCommand));

  // FX
  vkCmdBindPipeline(GAME_VK_COMMAND_BUFFER, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GAME_VK_FX_PIPELINE);
  pc.instance_buffer = GAME_FX_INSTANCE_BUFFER.bdaInstanceBuffer;
  vkCmdPushConstants(GAME_VK_COMMAND_BUFFER, GAME_VK_PIPELINE_LAYOUT,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &pc);
  vkCmdDrawIndirect(GAME_VK_COMMAND_BUFFER, GAME_VK_ALL_THE_DATA,
                    GAME_FX_DRAW_COMMANDS.bdaBufferOffset, 1,
                    sizeof(VkDrawIndirectCommand));
}

void vlk_endDraw() {
  vkCmdEndRendering(GAME_VK_COMMAND_BUFFER);
  vkCmdPipelineBarrier2(
      GAME_VK_COMMAND_BUFFER,
      &(VkDependencyInfo){
          .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 2,
          .pImageMemoryBarriers    = (VkImageMemoryBarrier2[]){
              {.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
               .image     = GAME_SWAPCHAIN_IMAGES[GAME_CURRENT_SWAPCHAIN_IMAGE],
               .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
               .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               .dstStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
               .subresourceRange =
                   (VkImageSubresourceRange){
                       .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                       .baseArrayLayer = 0,
                       .baseMipLevel   = 0,
                       .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                       .levelCount     = VK_REMAINING_MIP_LEVELS}},
              {.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
               .image         = GAME_VK_DEPTH_IMAGE,
               .oldLayout     = VK_IMAGE_LAYOUT_GENERAL,
               .newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
               .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
               .dstStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
               .subresourceRange = (VkImageSubresourceRange){
                   .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                   .baseArrayLayer = 0,
                   .baseMipLevel   = 0,
                   .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                   .levelCount     = VK_REMAINING_MIP_LEVELS}}}});
  vkEndCommandBuffer(GAME_VK_COMMAND_BUFFER);

  vkQueueSubmit(GAME_VK_PRESENT_QUEUE, 1,
                &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .commandBufferCount = 1,
                                .pCommandBuffers    = &GAME_VK_COMMAND_BUFFER},
                GAME_PRESENT_FENCE);
  vkWaitForFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE, VK_TRUE, 10000000);
  vkResetFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE);

  VkResult res = vkQueuePresentKHR(
      GAME_VK_PRESENT_QUEUE,
      &(VkPresentInfoKHR){.sType          = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                          .swapchainCount = 1,
                          .pImageIndices =
                              (const uint32_t[]){GAME_CURRENT_SWAPCHAIN_IMAGE},
                          .pSwapchains = &GAME_VK_SWAPCHAIN});
  if (res == VK_SUBOPTIMAL_KHR) {
    vlk_createSwapchain();
  }
}

#endif // VLK_HEADER
