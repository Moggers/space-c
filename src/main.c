#include "./engine.h"
#include "./vlk.h"
#include "vendor/cglm/affine-pre.h"
#include "vendor/cglm/mat4.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>
#include <assert.h>
#include <bits/time.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
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
  vlk_createSwapchain();
  vlk_createPipelines();

  uint32_t ship_model    = load_model("./bin/assets/ship.obj");
  uint32_t station_model = load_model("./bin/assets/station.obj");
  mat4 spawn_location;
  glm_mat4_identity(spawn_location);
  uint32_t station_a =
      make_entity(spawn_location, 1, (vec3){0., 1., 0.}, -1, -1, station_model);
  uint32_t player_ship =
      make_entity(spawn_location, 1, (vec3){0., 1., 0.}, 1, 100, ship_model);
  give_gun(player_ship, 1000, 0.2, ship_model);
  glm_translate(spawn_location, (vec3){1000., 0., 0.});
  uint32_t station_b =
      make_entity(spawn_location, 2, (vec3){0., 0., 1.}, -1, -1, station_model);
  uint32_t ship_b =
      make_entity(spawn_location, 2, (vec3){0., 0., 1.}, 1, 100, ship_model);
  give_gun(ship_b, 1000, 0.2, ship_model);
  PLAYER_CONTROLLED_ENTITY = player_ship;
  float time;
  float time_since_last_spawn = 0;

  while (!done) {

    // EVENTS
    float delta_t_f32 = ((float)(SDL_GetTicksNS() - time)) / 1e9f;
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
        if (!PLAYER_CONTROLLED_ENTITY) {
          glm_rotate_y(GAME_CAM_TRANSFORM, event.motion.xrel / 1000.,
                       GAME_CAM_TRANSFORM);
          glm_rotate_x(GAME_CAM_TRANSFORM, event.motion.yrel / 1000.,
                       GAME_CAM_TRANSFORM);
        } else {
          // TODO: Get mouse position in -1,1 coords, use to drive ship turn
          int winheight, winwidth;
          SDL_GetWindowSize(window, &winwidth, &winheight);
          float mousex = fmin(
              fmax((((float)event.motion.x / (float)winwidth) - 0.5) * 4, -1.),
              1.);
          float mousey = fmin(
              fmax((((float)event.motion.y / (float)winheight) - 0.5) * 4, -1.),
              1.);
          ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][0] = mousey;
          ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][1] = mousex;
        }
        break;
      }
      }
    }

    time_since_last_spawn += delta_t_f32;
    if (time_since_last_spawn > 2) {

      uint32_t owning_faction = (rand() % 2) + 1;
      uint32_t station        = station_a;
      if (owning_faction == 2) {
        station = station_b;
      }
      printf("Spawning ship\n");
      mat4 rotation;
      glm_mat4_copy(ENTITY_TRANSFORM[station_a], rotation);
      glm_vec4_zero(rotation[3]);
      vec3 offset = {100., 0., 0.};
      glm_mat4_mulv3(rotation, offset, 1, offset);
      glm_vec3_add(offset, ENTITY_TRANSFORM[station_a][3], offset);
      mat4 spawn_location;
      glm_mat4_copy(ENTITY_TRANSFORM[station], spawn_location);
      glm_translate(spawn_location, offset);
      uint32_t new_ship =
          make_entity(spawn_location, ENTITY_FACTION[station],
                      ENTITY_COLORS[station], 1, 100, ship_model);
      give_gun(new_ship, 1000, 0.2, ship_model);
      time_since_last_spawn = 0;
    }

    // WORLD
    glm_rotate_y(ENTITY_TRANSFORM[station_a], 0.1f * delta_t_f32,
                 ENTITY_TRANSFORM[station_a]);
    glm_rotate_y(ENTITY_TRANSFORM[station_b], 0.1f * delta_t_f32,
                 ENTITY_TRANSFORM[station_b]);

    // AI routines
    ai_select_targets();
    ai_thrusters();
    ai_shoot();

    // Physis
    apply_vec_thrusters(delta_t_f32);
    apply_thrusters(delta_t_f32);
    apply_movement(delta_t_f32);
    do_collisions();

    // Guns
    fire_guns(delta_t_f32);

    // GRAPHICS
    vlk_queueModelDrawCommands(ENTITY_COUNT, ENTITY_TRANSFORM, ENTITY_COLORS,
                               ENTITY_SCALE, ENTITY_MODEL);
    vlk_queueFxDrawCommands(FX_COUNT, FX_LOCATION, FX_T);

    // Freecam
    if (!PLAYER_CONTROLLED_ENTITY) {
      vec3 trans;
      SDL_Keymod mod  = SDL_GetModState();
      float movespeed = ((mod & SDL_KMOD_LSHIFT) == SDL_KMOD_LSHIFT) * 20 + 10;
      vec3 movedir    = {
          (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) * delta_t_f32 * 10 *
              movespeed,
          (keys[SDL_SCANCODE_X] - keys[SDL_SCANCODE_Z]) * delta_t_f32 * 10 *
              movespeed,
          (keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]) * delta_t_f32 * 10 *
              movespeed,
      };
      glm_translate(GAME_CAM_TRANSFORM, movedir);
    } else {
      ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][2] =
          (keys[SDL_SCANCODE_A] - keys[SDL_SCANCODE_D]) *
          ENTITY_VEC_THRUST[PLAYER_CONTROLLED_ENTITY];
      ENTITY_CURRENT_THRUST[PLAYER_CONTROLLED_ENTITY] =
          ENTITY_THRUST_POWER[PLAYER_CONTROLLED_ENTITY] * keys[SDL_SCANCODE_W];
      glm_mat4_copy(ENTITY_TRANSFORM[PLAYER_CONTROLLED_ENTITY],
                    GAME_CAM_TRANSFORM);
      glm_translate(GAME_CAM_TRANSFORM, (vec3){0., 4., -10.});

      ENTITY_GUN[PLAYER_CONTROLLED_ENTITY].active = keys[SDL_SCANCODE_SPACE];
    }

    if (vlk_beginDraw() != 0)
      continue;

    vlk_issueDraws();
    vlk_endDraw();
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
