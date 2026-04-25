#include "./vendor/fast_obj.h"
#include "./vlk.h"
#include "vendor/cglm/affine-pre.h"
#include "vendor/cglm/cglm.h"
#include "vendor/cglm/vec3.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#define FAST_OBJ_IMPLEMENTATION
#include "./vendor/fast_obj.h"

int main(int argc, char *argv[]) {

  bool done = false;
  SDL_Init(SDL_INIT_VIDEO); // Initialize SDL3
  const bool *keys = SDL_GetKeyboardState(0);

  SDL_Window *window = SDL_CreateWindow(
      "An SDL3 window", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  SDL_SetWindowRelativeMouseMode(window, false);
  assert(window != NULL);

  vlk_init(window);
  vlk_createGraphicsPipeline();

  fastObjMesh *brush = fast_obj_read("./bin/assets/ship.obj");
  assert(brush != 0);
  SomeShitAllocated index_buffer;
  unsigned int index_count = obj_to_indexbuffer(brush, &index_buffer);
  SomeShitAllocated vertex_buffer;
  vlk_allocateSomeShit(sizeof(float) * 3 * brush->position_count, &vertex_buffer);
  memcpy(vertex_buffer.the_shit_on_host, brush->positions, brush->position_count * 3 * sizeof(float));

  float time;

  while (!done) {
    float delta_t_f32 = ((float)(SDL_GetTicksNS() - time)) / 1e9f;
    float time_f32    = (float)(SDL_GetTicksNS()) / 1e9f;
    time              = SDL_GetTicksNS();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_WINDOW_RESIZED: {
        printf("EVENT_RESIZED %d %d\n", event.window.data1, event.window.data2);
        vlk_createSwapchain();
        break;
      }
      case SDL_EVENT_QUIT: {
        done = true;
        break;
      }
      case SDL_EVENT_MOUSE_MOTION: {
        glm_rotate_y(GAME_CAM_ROT, event.motion.xrel / 1000., GAME_CAM_ROT);
        glm_rotate_x(GAME_CAM_ROT, event.motion.yrel / 1000., GAME_CAM_ROT);
        break;
      }
      }
    }

    // Movedir
    vec3 trans;
    vec3 movedir = {(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) * delta_t_f32,
                    (keys[SDL_SCANCODE_Z] - keys[SDL_SCANCODE_X]) * delta_t_f32,
                    (keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]) *
                        delta_t_f32};
    glm_vec3_rotate_m4(GAME_CAM_ROT, movedir, trans);
    glm_vec3_add(GAME_CAM_POS, trans, GAME_CAM_POS);

    if (vlk_beginDraw() != 0)
      continue;
    vkCmdBindIndexBuffer(GAME_VK_COMMAND_BUFFER, GAME_VK_ALL_THE_DATA, index_buffer.offset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(GAME_VK_COMMAND_BUFFER, index_count, 1, 0, 0, 0);
    vkCmdDraw(GAME_VK_COMMAND_BUFFER, 3, 1, 0, 0);
    vlk_endDraw();
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
