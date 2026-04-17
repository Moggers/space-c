#include "./vendor/fast_obj.h"
#include "./vlk.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#define FAST_OBJ_IMPLEMENTATION
#include "./vendor/fast_obj.h"

int main(int argc, char *argv[]) {

  bool done = false;
  VkResult res;
  SDL_Init(SDL_INIT_VIDEO); // Initialize SDL3

  SDL_Window *window = SDL_CreateWindow(
      "An SDL3 window", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  assert(window != NULL);

  vlk_Init(window);
  vlk_createGraphicsPipeline();

  fastObjMesh *brush = fast_obj_read("./assets/unnamed.obj");


  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_WINDOW_RESIZED)
        vlk_createSwapchain();
      if (event.type == SDL_EVENT_QUIT)
        done = true;
    }

    if (vlk_beginDraw() != 0)
      continue;
    vkCmdDraw(GAME_VK_COMMAND_BUFFER, 3, 1, 0, 0);
    vlk_endDraw();
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
