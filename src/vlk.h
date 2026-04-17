#ifndef VLK_HEADER
#define VLK_HEADER
#include "./vendor/fast_obj.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#define VK_WRAP(expr)                                                          \
  {                                                                            \
    VkResult res = expr;                                                       \
    assert(res == VK_SUCCESS);                                                 \
  }

#define VK_WRAP_ARR(type, name, fn, ...)                                       \
  uint32_t name##_count = 0;                                                   \
  fn(__VA_ARGS__, &name##_count, 0);                                           \
  type name[name##_count];                                                     \
  fn(__VA_ARGS__, &name##_count, name);

VkInstance GAME_VK_INSTANCE                         = VK_NULL_HANDLE;
VkSurfaceKHR GAME_VK_SURFACE                        = VK_NULL_HANDLE;
VkDevice GAME_VK_DEVICE                             = VK_NULL_HANDLE;
VkPhysicalDevice GAME_VK_PHYSICAL_DEVICE            = VK_NULL_HANDLE;
VkSwapchainKHR GAME_VK_SWAPCHAIN                    = VK_NULL_HANDLE;
VkQueue GAME_VK_PRESENT_QUEUE                       = VK_NULL_HANDLE;
VkCommandPool GAME_VK_COMMAND_POOL                  = VK_NULL_HANDLE;
VkCommandBuffer GAME_VK_COMMAND_BUFFER              = VK_NULL_HANDLE;
VkPipeline GAME_VK_PIPELINE                         = VK_NULL_HANDLE;
VkPipelineLayout GAME_VK_PIPELINE_LAYOUT            = VK_NULL_HANDLE;
VkDescriptorPool GAME_VK_DESCRIPTOR_POOL            = VK_NULL_HANDLE;
VkDescriptorSet GAME_VK_DESCRIPTOR_SET              = VK_NULL_HANDLE;
VkDescriptorSetLayout GAME_VK_DESCRIPTOR_SET_LAYOUT = VK_NULL_HANDLE;
VkShaderModule GAME_VERT_MODULE                     = VK_NULL_HANDLE;
VkShaderModule GAME_FRAG_MODULE                     = VK_NULL_HANDLE;
VkSurfaceFormatKHR GAME_VK_SURFACE_FORMAT;
VkImage GAME_SWAPCHAIN_IMAGES[32];
uint32_t GAME_SWAPCHAIN_IMAGE_COUNT = 32;
VkImageView GAME_SWAPCHAIN_IMAGE_VIEWS[32];
VkSurfaceCapabilitiesKHR GAME_SURFACE_CAPABILITIES;
VkFence GAME_PRESENT_FENCE;
uint32_t GAME_CURRENT_SWAPCHAIN_IMAGE = 0;

void vlk_createShaderModule(char *path, VkShaderModule *module) {
  FILE *f = fopen(path, "r");
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

// Returns 1 on failure
VkResult vlk_createSwapchain() {
  VkResult res;
  VkSwapchainKHR old = GAME_VK_SWAPCHAIN;
  res                = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      GAME_VK_PHYSICAL_DEVICE, GAME_VK_SURFACE, &GAME_SURFACE_CAPABILITIES);
  if (res != VK_SUCCESS)
    return res;
  res = vkCreateSwapchainKHR(
      GAME_VK_DEVICE,
      &(VkSwapchainCreateInfoKHR){
          .oldSwapchain     = GAME_VK_SWAPCHAIN,
          .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
          .surface          = GAME_VK_SURFACE,
          .minImageCount    = GAME_SURFACE_CAPABILITIES.minImageCount,
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
  if (old != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(GAME_VK_DEVICE);
    vkDestroySwapchainKHR(GAME_VK_DEVICE, old, 0);
  }
  return VK_SUCCESS;
}

void vlk_Init(SDL_Window *window) {

  VK_WRAP(vkCreateInstance(
      &(VkInstanceCreateInfo){
          .sType                 = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
          .enabledExtensionCount = 2,
          .pApplicationInfo =
              &(VkApplicationInfo){.apiVersion = VK_API_VERSION_1_4},
          .ppEnabledExtensionNames =
              (const char *[]){
                  "VK_KHR_xlib_surface",
                  "VK_KHR_surface",
              },
      },
      0, &GAME_VK_INSTANCE));

  if (!SDL_Vulkan_CreateSurface(window, GAME_VK_INSTANCE, 0,
                                &GAME_VK_SURFACE)) {
    printf("Couldnt make sur: %s\n", SDL_GetError());
  }
  // DEVICE
  uint32_t device_count;
  VK_WRAP(vkEnumeratePhysicalDevices(GAME_VK_INSTANCE, &device_count, 0));
  VkPhysicalDevice pdevices[device_count];
  VK_WRAP(
      vkEnumeratePhysicalDevices(GAME_VK_INSTANCE, &device_count, pdevices));
  for (int i = 0; i < device_count; i++) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(pdevices[i], &properties);
    printf("Device %d: %s\n", i, properties.deviceName);
  }
  GAME_VK_PHYSICAL_DEVICE = pdevices[0];
  VK_WRAP(vkCreateDevice(
      pdevices[0],
      &(VkDeviceCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
          .pNext =
              &(VkPhysicalDeviceDynamicRenderingFeatures){
                  .sType =
                      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
                  .dynamicRendering = VK_TRUE,
                  .pNext =
                      &(VkPhysicalDeviceSynchronization2Features){
                          .sType =
                              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
                          .synchronization2 = VK_TRUE}},
          .queueCreateInfoCount  = 1,
          .enabledExtensionCount = 5,
          .ppEnabledExtensionNames =
              (const char *[]){
                  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                  VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
                  VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                  VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
                  VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
              },
          .pQueueCreateInfos =
              &(const VkDeviceQueueCreateInfo){
                  .sType      = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                  .queueCount = 1,
                  .pQueuePriorities = &(float){1.f}}},
      0, &GAME_VK_DEVICE));
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
}

void vlk_createGraphicsPipeline() {

  VK_WRAP_ARR(VkSurfaceFormatKHR, formats, vkGetPhysicalDeviceSurfaceFormatsKHR,
              GAME_VK_PHYSICAL_DEVICE, GAME_VK_SURFACE);
  GAME_VK_SURFACE_FORMAT = formats[0];
  vkCreateDescriptorPool(
      GAME_VK_DEVICE,
      &(VkDescriptorPoolCreateInfo){
          .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
          .poolSizeCount = 0,
          .maxSets       = 1},
      0, &GAME_VK_DESCRIPTOR_POOL);
  VK_WRAP(vkCreateDescriptorSetLayout(
      GAME_VK_DEVICE,
      &(VkDescriptorSetLayoutCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
      0, &GAME_VK_DESCRIPTOR_SET_LAYOUT));
  VK_WRAP(vkAllocateDescriptorSets(
      GAME_VK_DEVICE,
      &(VkDescriptorSetAllocateInfo){
          .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .descriptorSetCount = 1,
          .pSetLayouts        = &GAME_VK_DESCRIPTOR_SET_LAYOUT,
          .descriptorPool     = GAME_VK_DESCRIPTOR_POOL},
      &GAME_VK_DESCRIPTOR_SET));
  vkCreatePipelineLayout(
      GAME_VK_DEVICE,
      &(VkPipelineLayoutCreateInfo){
          .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .setLayoutCount = 1,
          .pSetLayouts    = &GAME_VK_DESCRIPTOR_SET_LAYOUT},
      0, &GAME_VK_PIPELINE_LAYOUT);

  vlk_createShaderModule("./bin/vertex.spv", &GAME_VERT_MODULE);
  vlk_createShaderModule("./bin/fragment.spv", &GAME_FRAG_MODULE);

  vkCreateGraphicsPipelines(
      GAME_VK_DEVICE, VK_NULL_HANDLE, 1,
      &(VkGraphicsPipelineCreateInfo){
          .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
          .pNext =
              &(VkPipelineRenderingCreateInfoKHR){
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                  .colorAttachmentCount    = 1,
                  .pColorAttachmentFormats = &GAME_VK_SURFACE_FORMAT.format},
          .pRasterizationState =
              &(VkPipelineRasterizationStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                  .lineWidth = 1.},
          .pColorBlendState =
              &(VkPipelineColorBlendStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                  .attachmentCount = 1,
                  .pAttachments =
                      &(VkPipelineColorBlendAttachmentState){
                          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
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
                  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP},
          .pVertexInputState =
              &(VkPipelineVertexInputStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO},
          .pStages =
              (VkPipelineShaderStageCreateInfo[2]){
                  {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                   .stage = VK_SHADER_STAGE_VERTEX_BIT,
                   .pName = "main",
                   .module = GAME_VERT_MODULE},
                  {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                   .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .pName = "main",
                   .module = GAME_FRAG_MODULE}

              }},
      0, &GAME_VK_PIPELINE);
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
  if (res != VK_SUCCESS)
    return 1;
  vkBeginCommandBuffer(GAME_VK_COMMAND_BUFFER,
                       &(VkCommandBufferBeginInfo){
                           .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                       });
  vkCmdPipelineBarrier2(
      GAME_VK_COMMAND_BUFFER,
      &(VkDependencyInfo){
          .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers    = &(VkImageMemoryBarrier2){
              .sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .image     = GAME_SWAPCHAIN_IMAGES[GAME_CURRENT_SWAPCHAIN_IMAGE],
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_GENERAL,
              .subresourceRange = (VkImageSubresourceRange){
                  .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseArrayLayer = 0,
                  .baseMipLevel   = 0,
                  .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                  .levelCount     = VK_REMAINING_MIP_LEVELS}}});
  vkCmdBindPipeline(GAME_VK_COMMAND_BUFFER, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GAME_VK_PIPELINE);
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
          .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .colorAttachmentCount = 1,
          .layerCount           = 1,
          .pColorAttachments =
              &(VkRenderingAttachmentInfo){
                  .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                  .clearValue =
                      (VkClearValue){.color =
                                         (VkClearColorValue){
                                             .float32 = {0.1, 0.1, 0.1, 1.}}},
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                  .imageView =
                      GAME_SWAPCHAIN_IMAGE_VIEWS[GAME_CURRENT_SWAPCHAIN_IMAGE],
                  .loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE},
          .renderArea =
              (VkRect2D){.extent = GAME_SURFACE_CAPABILITIES.currentExtent,
                         .offset = (VkOffset2D){.x = 0, .y = 0}}});
  return 0;
}
void vlk_endDraw() {
  vkCmdEndRendering(GAME_VK_COMMAND_BUFFER);
  vkCmdPipelineBarrier2(
      GAME_VK_COMMAND_BUFFER,
      &(VkDependencyInfo){
          .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers    = &(VkImageMemoryBarrier2){
              .sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .image     = GAME_SWAPCHAIN_IMAGES[GAME_CURRENT_SWAPCHAIN_IMAGE],
              .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
              .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
              .subresourceRange = (VkImageSubresourceRange){
                  .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseArrayLayer = 0,
                  .baseMipLevel   = 0,
                  .layerCount     = VK_REMAINING_ARRAY_LAYERS,
                  .levelCount     = VK_REMAINING_MIP_LEVELS}}});
  vkEndCommandBuffer(GAME_VK_COMMAND_BUFFER);

  vkQueueSubmit(GAME_VK_PRESENT_QUEUE, 1,
                &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .commandBufferCount = 1,
                                .pCommandBuffers    = &GAME_VK_COMMAND_BUFFER},
                GAME_PRESENT_FENCE);
  vkWaitForFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE, VK_TRUE, 10000000);
  vkResetFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE);

  vkQueuePresentKHR(
      GAME_VK_PRESENT_QUEUE,
      &(VkPresentInfoKHR){.sType          = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                          .swapchainCount = 1,
                          .pImageIndices =
                              (const uint32_t[]){GAME_CURRENT_SWAPCHAIN_IMAGE},
                          .pSwapchains = &GAME_VK_SWAPCHAIN});
  vkResetFences(GAME_VK_DEVICE, 1, &GAME_PRESENT_FENCE);
}

#endif // VLK_HEADER
