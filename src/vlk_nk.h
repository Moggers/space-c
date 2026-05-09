#ifndef VLK_NK_HEADER
#define VLK_NK_HEADER

#include "vlk.h"
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <math.h>
#include <string.h>

#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "./vendor/nuklear.h"

#define VLK_NK_MAX_VERTEX_BUFFER (512u * 1024u)
#define VLK_NK_MAX_INDEX_BUFFER  (128u * 1024u)

typedef struct VlkNkVertex {
  float    pos[2];
  float    uv[2];
  uint32_t col;
} VlkNkVertex;

typedef struct VlkNkPushConstant {
  float           viewport[2];
  VkDeviceAddress vertex_buffer;
} VlkNkPushConstant;

// Nuklear state
struct nk_context           GAME_NK_CTX;
struct nk_font_atlas        GAME_NK_ATLAS;
struct nk_buffer            GAME_NK_CMDS;
struct nk_draw_null_texture GAME_NK_NULL_TEX;

// Vulkan state for UI
VkPipeline            GAME_VK_UI_PIPELINE        = VK_NULL_HANDLE;
VkPipelineLayout      GAME_VK_UI_PIPELINE_LAYOUT = VK_NULL_HANDLE;
VkDescriptorSetLayout GAME_VK_UI_DSL             = VK_NULL_HANDLE;
VkDescriptorPool      GAME_VK_UI_DPOOL           = VK_NULL_HANDLE;
VkDescriptorSet       GAME_VK_UI_DSET            = VK_NULL_HANDLE;
VkSampler             GAME_VK_UI_SAMPLER         = VK_NULL_HANDLE;
VkImage               GAME_VK_UI_FONT_IMAGE      = VK_NULL_HANDLE;
VkImageView           GAME_VK_UI_FONT_VIEW       = VK_NULL_HANDLE;
VkDeviceMemory        GAME_VK_UI_FONT_MEM        = VK_NULL_HANDLE;

SomeShitAllocated GAME_NK_VTX_ARENA;
SomeShitAllocated GAME_NK_IDX_ARENA;

static void vlk_nk_createUiPipeline(void) {
  VkShaderModule vert, frag;
  vlk_createShaderModule("./shaders/ui_vertex.spv", &vert);
  vlk_createShaderModule("./shaders/ui_fragment.spv", &frag);

  VK_WRAP(vkCreateGraphicsPipelines(
      GAME_VK_DEVICE, VK_NULL_HANDLE, 1,
      &(VkGraphicsPipelineCreateInfo){
          .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
          .pNext =
              &(VkPipelineRenderingCreateInfoKHR){
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                  .colorAttachmentCount    = 1,
                  .pColorAttachmentFormats = &GAME_VK_SURFACE_FORMAT.format,
                  .depthAttachmentFormat   = VK_FORMAT_D32_SFLOAT},
          .pRasterizationState =
              &(VkPipelineRasterizationStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                  .lineWidth = 1.0f,
                  .cullMode  = VK_CULL_MODE_NONE,
                  .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                  .polygonMode = VK_POLYGON_MODE_FILL},
          .pDepthStencilState =
              &(VkPipelineDepthStencilStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                  .depthTestEnable   = VK_FALSE,
                  .depthWriteEnable  = VK_FALSE,
                  .stencilTestEnable = VK_FALSE},
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
                          .dstAlphaBlendFactor =
                              VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                          .alphaBlendOp   = VK_BLEND_OP_ADD,
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
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                  .scissorCount  = 1,
                  .viewportCount = 1},
          .pMultisampleState =
              &(VkPipelineMultisampleStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                  .rasterizationSamples = 1},
          .stageCount = 2,
          .layout     = GAME_VK_UI_PIPELINE_LAYOUT,
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
                   .pName  = "main",
                   .module = vert},
                  {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                   .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                   .pName  = "main",
                   .module = frag}}},
      0, &GAME_VK_UI_PIPELINE));
}

static void vlk_nk_uploadFontTexture(const void *pixels, uint32_t w,
                                     uint32_t h) {
  // Stage atlas through the host-visible arena.
  SomeShitAllocated stage;
  vlk_allocateSomeShit((size_t)w * (size_t)h * 4u, &stage);
  memcpy(stage.the_shit_on_host, pixels, (size_t)w * (size_t)h * 4u);

  VK_WRAP(vkCreateImage(
      GAME_VK_DEVICE,
      &(VkImageCreateInfo){
          .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType     = VK_IMAGE_TYPE_2D,
          .arrayLayers   = 1,
          .mipLevels     = 1,
          .samples       = 1,
          .format        = VK_FORMAT_R8G8B8A8_UNORM,
          .extent        = (VkExtent3D){w, h, 1},
          .tiling        = VK_IMAGE_TILING_OPTIMAL,
          .usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED},
      0, &GAME_VK_UI_FONT_IMAGE));

  VkMemoryRequirements mreq;
  vkGetImageMemoryRequirements(GAME_VK_DEVICE, GAME_VK_UI_FONT_IMAGE, &mreq);
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(GAME_VK_PHYSICAL_DEVICE, &mp);
  int mt = -1;
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
    if ((mreq.memoryTypeBits & (1u << i)) &&
        (mp.memoryTypes[i].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
      mt = (int)i;
      break;
    }
  }
  assert(mt != -1);
  VK_WRAP(vkAllocateMemory(
      GAME_VK_DEVICE,
      &(VkMemoryAllocateInfo){.sType =
                                  VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                              .allocationSize  = mreq.size,
                              .memoryTypeIndex = (uint32_t)mt},
      0, &GAME_VK_UI_FONT_MEM));
  vkBindImageMemory(GAME_VK_DEVICE, GAME_VK_UI_FONT_IMAGE, GAME_VK_UI_FONT_MEM,
                    0);

  VK_WRAP(vkCreateImageView(
      GAME_VK_DEVICE,
      &(VkImageViewCreateInfo){
          .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image    = GAME_VK_UI_FONT_IMAGE,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format   = VK_FORMAT_R8G8B8A8_UNORM,
          .subresourceRange =
              (VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                        .layerCount = 1,
                                        .levelCount = 1}},
      0, &GAME_VK_UI_FONT_VIEW));

  // One-shot upload using the existing command buffer.
  VkImageSubresourceRange full = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .layerCount = 1,
                                  .levelCount = 1};
  vkBeginCommandBuffer(
      GAME_VK_COMMAND_BUFFER,
      &(VkCommandBufferBeginInfo){
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});
  vkCmdPipelineBarrier2(
      GAME_VK_COMMAND_BUFFER,
      &(VkDependencyInfo){
          .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers =
              &(VkImageMemoryBarrier2){
                  .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                  .image            = GAME_VK_UI_FONT_IMAGE,
                  .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                  .newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  .srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                  .dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .dstAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  .subresourceRange = full}});
  vkCmdCopyBufferToImage(
      GAME_VK_COMMAND_BUFFER, GAME_VK_ALL_THE_DATA, GAME_VK_UI_FONT_IMAGE,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
      &(VkBufferImageCopy){
          .bufferOffset = stage.offset_in_buffer,
          .imageSubresource =
              (VkImageSubresourceLayers){.aspectMask =
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                         .layerCount = 1},
          .imageExtent = (VkExtent3D){w, h, 1}});
  vkCmdPipelineBarrier2(
      GAME_VK_COMMAND_BUFFER,
      &(VkDependencyInfo){
          .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers =
              &(VkImageMemoryBarrier2){
                  .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                  .image            = GAME_VK_UI_FONT_IMAGE,
                  .oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  .newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  .srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  .dstStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  .dstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT,
                  .subresourceRange = full}});
  vkEndCommandBuffer(GAME_VK_COMMAND_BUFFER);
  vkQueueSubmit(GAME_VK_PRESENT_QUEUE, 1,
                &(VkSubmitInfo){.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .commandBufferCount = 1,
                                .pCommandBuffers = &GAME_VK_COMMAND_BUFFER},
                VK_NULL_HANDLE);
  vkQueueWaitIdle(GAME_VK_PRESENT_QUEUE);
}

struct nk_context *vlk_nk_init(void) {
  // Linear sampler with clamp-to-edge for the glyph atlas.
  VK_WRAP(vkCreateSampler(
      GAME_VK_DEVICE,
      &(VkSamplerCreateInfo){
          .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
          .magFilter    = VK_FILTER_LINEAR,
          .minFilter    = VK_FILTER_LINEAR,
          .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR},
      0, &GAME_VK_UI_SAMPLER));

  VK_WRAP(vkCreateDescriptorSetLayout(
      GAME_VK_DEVICE,
      &(VkDescriptorSetLayoutCreateInfo){
          .sType =
              VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .bindingCount = 1,
          .pBindings = &(VkDescriptorSetLayoutBinding){
              .binding         = 0,
              .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT}},
      0, &GAME_VK_UI_DSL));

  VK_WRAP(vkCreateDescriptorPool(
      GAME_VK_DEVICE,
      &(VkDescriptorPoolCreateInfo){
          .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
          .maxSets       = 1,
          .poolSizeCount = 1,
          .pPoolSizes    = &(VkDescriptorPoolSize){
              .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1}},
      0, &GAME_VK_UI_DPOOL));

  VK_WRAP(vkAllocateDescriptorSets(
      GAME_VK_DEVICE,
      &(VkDescriptorSetAllocateInfo){
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .descriptorPool     = GAME_VK_UI_DPOOL,
          .descriptorSetCount = 1,
          .pSetLayouts        = &GAME_VK_UI_DSL},
      &GAME_VK_UI_DSET));

  VK_WRAP(vkCreatePipelineLayout(
      GAME_VK_DEVICE,
      &(VkPipelineLayoutCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .setLayoutCount         = 1,
          .pSetLayouts            = &GAME_VK_UI_DSL,
          .pushConstantRangeCount = 1,
          .pPushConstantRanges    = &(VkPushConstantRange){
              .offset     = 0,
              .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
              .size       = sizeof(VlkNkPushConstant)}},
      0, &GAME_VK_UI_PIPELINE_LAYOUT));

  vlk_nk_createUiPipeline();

  // Reusable per-frame vertex / index slabs in the arena.
  vlk_allocateSomeShit(VLK_NK_MAX_VERTEX_BUFFER, &GAME_NK_VTX_ARENA);
  vlk_allocateSomeShit(VLK_NK_MAX_INDEX_BUFFER, &GAME_NK_IDX_ARENA);

  nk_init_default(&GAME_NK_CTX, 0);
  nk_buffer_init_default(&GAME_NK_CMDS);

  // Bake the default font and upload it.
  nk_font_atlas_init_default(&GAME_NK_ATLAS);
  nk_font_atlas_begin(&GAME_NK_ATLAS);
  int                fw = 0, fh = 0;
  const void        *image =
      nk_font_atlas_bake(&GAME_NK_ATLAS, &fw, &fh, NK_FONT_ATLAS_RGBA32);
  vlk_nk_uploadFontTexture(image, (uint32_t)fw, (uint32_t)fh);
  nk_font_atlas_end(&GAME_NK_ATLAS, nk_handle_id(0), &GAME_NK_NULL_TEX);
  if (GAME_NK_ATLAS.default_font) {
    nk_style_set_font(&GAME_NK_CTX, &GAME_NK_ATLAS.default_font->handle);
  }

  vkUpdateDescriptorSets(
      GAME_VK_DEVICE, 1,
      &(VkWriteDescriptorSet){
          .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet          = GAME_VK_UI_DSET,
          .dstBinding      = 0,
          .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .pImageInfo      = &(VkDescriptorImageInfo){
              .sampler     = GAME_VK_UI_SAMPLER,
              .imageView   = GAME_VK_UI_FONT_VIEW,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}},
      0, 0);

  return &GAME_NK_CTX;
}

static inline void vlk_nk_inputBegin(void) { nk_input_begin(&GAME_NK_CTX); }
static inline void vlk_nk_inputEnd(void)   { nk_input_end(&GAME_NK_CTX); }

void vlk_nk_handleEvent(SDL_Event *e) {
  struct nk_context *ctx = &GAME_NK_CTX;
  switch (e->type) {
  case SDL_EVENT_MOUSE_MOTION:
    nk_input_motion(ctx, (int)e->motion.x, (int)e->motion.y);
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    int             down = (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
    enum nk_buttons b    = NK_BUTTON_LEFT;
    if (e->button.button == SDL_BUTTON_RIGHT)       b = NK_BUTTON_RIGHT;
    else if (e->button.button == SDL_BUTTON_MIDDLE) b = NK_BUTTON_MIDDLE;
    nk_input_button(ctx, b, (int)e->button.x, (int)e->button.y, down);
    break;
  }
  case SDL_EVENT_MOUSE_WHEEL:
    nk_input_scroll(ctx, nk_vec2(e->wheel.x, e->wheel.y));
    break;
  case SDL_EVENT_TEXT_INPUT: {
    for (const char *c = e->text.text; *c;) {
      nk_glyph      g    = {0};
      int           len  = 1;
      unsigned char b0   = (unsigned char)*c;
      if      ((b0 & 0x80) == 0x00) len = 1;
      else if ((b0 & 0xE0) == 0xC0) len = 2;
      else if ((b0 & 0xF0) == 0xE0) len = 3;
      else if ((b0 & 0xF8) == 0xF0) len = 4;
      for (int i = 0; i < len && c[i]; i++) g[i] = c[i];
      nk_input_glyph(ctx, g);
      c += len;
    }
    break;
  }
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP: {
    int          down = (e->type == SDL_EVENT_KEY_DOWN);
    SDL_Keycode  k    = e->key.key;
    SDL_Keymod   mod  = e->key.mod;
    int          ctrl = (mod & SDL_KMOD_CTRL) != 0;
    switch (k) {
    case SDLK_LSHIFT: case SDLK_RSHIFT:
      nk_input_key(ctx, NK_KEY_SHIFT, down); break;
    case SDLK_LCTRL: case SDLK_RCTRL:
      nk_input_key(ctx, NK_KEY_CTRL, down); break;
    case SDLK_DELETE:    nk_input_key(ctx, NK_KEY_DEL, down); break;
    case SDLK_RETURN:    nk_input_key(ctx, NK_KEY_ENTER, down); break;
    case SDLK_TAB:       nk_input_key(ctx, NK_KEY_TAB, down); break;
    case SDLK_BACKSPACE: nk_input_key(ctx, NK_KEY_BACKSPACE, down); break;
    case SDLK_HOME:
      nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down);
      nk_input_key(ctx, NK_KEY_SCROLL_START, down);
      break;
    case SDLK_END:
      nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down);
      nk_input_key(ctx, NK_KEY_SCROLL_END, down);
      break;
    case SDLK_PAGEUP:    nk_input_key(ctx, NK_KEY_SCROLL_UP, down); break;
    case SDLK_PAGEDOWN:  nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down); break;
    case SDLK_LEFT:
      nk_input_key(ctx, ctrl ? NK_KEY_TEXT_WORD_LEFT : NK_KEY_LEFT, down);
      break;
    case SDLK_RIGHT:
      nk_input_key(ctx, ctrl ? NK_KEY_TEXT_WORD_RIGHT : NK_KEY_RIGHT, down);
      break;
    case SDLK_UP:        nk_input_key(ctx, NK_KEY_UP, down); break;
    case SDLK_DOWN:      nk_input_key(ctx, NK_KEY_DOWN, down); break;
    case SDLK_C: if (ctrl) nk_input_key(ctx, NK_KEY_COPY, down); break;
    case SDLK_V: if (ctrl) nk_input_key(ctx, NK_KEY_PASTE, down); break;
    case SDLK_X: if (ctrl) nk_input_key(ctx, NK_KEY_CUT, down); break;
    case SDLK_Z: if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_UNDO, down); break;
    case SDLK_Y: if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_REDO, down); break;
    case SDLK_A: if (ctrl) nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, down); break;
    }
    break;
  }
  }
}

void vlk_nk_render(void) {
  static const struct nk_draw_vertex_layout_element vlayout[] = {
      {NK_VERTEX_POSITION, NK_FORMAT_FLOAT,    NK_OFFSETOF(VlkNkVertex, pos)},
      {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT,    NK_OFFSETOF(VlkNkVertex, uv)},
      {NK_VERTEX_COLOR,    NK_FORMAT_R8G8B8A8, NK_OFFSETOF(VlkNkVertex, col)},
      {NK_VERTEX_LAYOUT_END}};

  struct nk_convert_config cfg = {0};
  cfg.vertex_layout            = vlayout;
  cfg.vertex_size              = sizeof(VlkNkVertex);
  cfg.vertex_alignment         = NK_ALIGNOF(VlkNkVertex);
  cfg.tex_null                 = GAME_NK_NULL_TEX;
  cfg.circle_segment_count     = 22;
  cfg.curve_segment_count      = 22;
  cfg.arc_segment_count        = 22;
  cfg.global_alpha             = 1.0f;
  cfg.shape_AA                 = NK_ANTI_ALIASING_ON;
  cfg.line_AA                  = NK_ANTI_ALIASING_ON;

  struct nk_buffer vb, ib;
  nk_buffer_init_fixed(&vb, GAME_NK_VTX_ARENA.the_shit_on_host,
                       VLK_NK_MAX_VERTEX_BUFFER);
  nk_buffer_init_fixed(&ib, GAME_NK_IDX_ARENA.the_shit_on_host,
                       VLK_NK_MAX_INDEX_BUFFER);
  nk_convert(&GAME_NK_CTX, &GAME_NK_CMDS, &vb, &ib, &cfg);

  vkCmdBindPipeline(GAME_VK_COMMAND_BUFFER, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    GAME_VK_UI_PIPELINE);
  vkCmdBindDescriptorSets(GAME_VK_COMMAND_BUFFER,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          GAME_VK_UI_PIPELINE_LAYOUT, 0, 1, &GAME_VK_UI_DSET, 0,
                          0);
  vkCmdBindIndexBuffer(GAME_VK_COMMAND_BUFFER, GAME_VK_ALL_THE_DATA,
                       GAME_NK_IDX_ARENA.offset_in_buffer,
                       VK_INDEX_TYPE_UINT16);

  uint32_t fb_w = GAME_SURFACE_CAPABILITIES.currentExtent.width;
  uint32_t fb_h = GAME_SURFACE_CAPABILITIES.currentExtent.height;
  VlkNkPushConstant pc = {.viewport      = {(float)fb_w, (float)fb_h},
                          .vertex_buffer =
                              GAME_NK_VTX_ARENA.the_shit_on_device};
  vkCmdPushConstants(GAME_VK_COMMAND_BUFFER, GAME_VK_UI_PIPELINE_LAYOUT,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

  uint32_t                    idx_offset = 0;
  const struct nk_draw_command *cmd;
  nk_draw_foreach(cmd, &GAME_NK_CTX, &GAME_NK_CMDS) {
    if (!cmd->elem_count) continue;
    int32_t sx = (int32_t)fmaxf(cmd->clip_rect.x, 0.0f);
    int32_t sy = (int32_t)fmaxf(cmd->clip_rect.y, 0.0f);
    int32_t sw = (int32_t)cmd->clip_rect.w;
    int32_t sh = (int32_t)cmd->clip_rect.h;
    if (sx > (int32_t)fb_w) sx = (int32_t)fb_w;
    if (sy > (int32_t)fb_h) sy = (int32_t)fb_h;
    if (sx + sw > (int32_t)fb_w) sw = (int32_t)fb_w - sx;
    if (sy + sh > (int32_t)fb_h) sh = (int32_t)fb_h - sy;
    if (sw < 0) sw = 0;
    if (sh < 0) sh = 0;
    vkCmdSetScissor(
        GAME_VK_COMMAND_BUFFER, 0, 1,
        &(VkRect2D){.offset = (VkOffset2D){sx, sy},
                    .extent = (VkExtent2D){(uint32_t)sw, (uint32_t)sh}});
    vkCmdDrawIndexed(GAME_VK_COMMAND_BUFFER, cmd->elem_count, 1, idx_offset, 0,
                     0);
    idx_offset += cmd->elem_count;
  }

  // Restore full-frame scissor so subsequent passes don't inherit a UI clip.
  vkCmdSetScissor(GAME_VK_COMMAND_BUFFER, 0, 1,
                  &(VkRect2D){.offset = (VkOffset2D){0, 0},
                              .extent = GAME_SURFACE_CAPABILITIES.currentExtent});

  nk_clear(&GAME_NK_CTX);
  nk_buffer_clear(&GAME_NK_CMDS);
}

#endif // VLK_NK_HEADER
