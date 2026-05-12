#define MAP_LOADER_IMPLEMENTATION
#define MAP_WINDING_CCW
#include "./map_loader.h"
#define BVH_IMPLEMENTATION
#include "./bvh.h"

#include "contracts.h"
#include "entities.h"
#include "physics.h"
#include "vendor/cglm/affine-pre.h"
#include "vendor/cglm/mat4.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <time.h>
#include <vulkan/vulkan_core.h>

#include "./engine.h"
#include "./faction.h"
#include "./ui.h"
#include "./vlk.h"
#include "./vlk_nk.h"
#include "engine.h"
#include "vendor/cglm/vec3.h"

#define FREECAM 0

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
  UI_CTX = vlk_nk_init();

  map_t asteroid_map, ship_map, station_map;
  map_load(&asteroid_map, "./assets/asteroid.map");
  map_load(&station_map, "./assets/station.map");
  map_load(&ship_map, "./assets/ship.map");

  uint32_t ship_model     = load_model(&ship_map);
  uint32_t station_model  = load_model(&station_map);
  uint32_t asteroid_model = load_model(&asteroid_map);
  model_set_bvh(ship_model, &ship_map);
  model_set_bvh(station_model, &station_map);
  model_set_bvh(asteroid_model, &asteroid_map);

  mat4 spawn_location;

  // Factions
  uint32_t player_faction = faction_create("Player");
  uint32_t mining_faction = faction_create("Mining Corp");

  // Asteroid
  glm_mat4_identity(spawn_location);
  glm_translate(spawn_location, (vec3){500, 0, 0});
  uint32_t asteroid =
      make_entity(spawn_location, 0, (vec3){0.3, 0.3, 0.3}, 0,
                  (vec3){-1, -1, -1}, asteroid_model, "Asteroid");
  entity_set_ondeath(asteroid, ONDEATH_SPLIT);
  entity_set_health(asteroid, 100);
  entity_set_scale(asteroid, (vec3){64, 64, 64});
  entity_set_asteroid(asteroid, 1);

  // Player
  glm_mat4_identity(spawn_location);
  glm_translate(spawn_location, (vec3){500, 100, -200});
  PLAYER_CONTROLLED_ENTITY =
      make_entity(spawn_location, player_faction, (vec3){0., 1., 0.}, 1,
                  (vec3){20, 20, 100}, ship_model, "Player Ship");
  give_gun(PLAYER_CONTROLLED_ENTITY, 1000, 0.2, ship_model, 1);
  entity_set_ondeath(PLAYER_CONTROLLED_ENTITY, ONDEATH_EXPLODE);
  entity_set_health(PLAYER_CONTROLLED_ENTITY, 5);
  //
  // First station
  glm_mat4_identity(spawn_location);
  glm_translate(spawn_location, (vec3){0, 0, 0});
  uint32_t station_a =
      make_entity(spawn_location, mining_faction, (vec3){0., 1., 0.}, -1,
                  (vec3){-1, -1, -1}, station_model, "Statio A");
  entity_set_health(station_a, 10000);
  entity_set_ondeath(station_a, ONDEATH_EXPLODE);
  entity_set_scale(station_a, (vec3){1., 1., 1.});
  contract_add(station_a, CONTRACT_ORE, "Deliver Ore", 1);

  // Rando AI ship
  glm_mat4_identity(spawn_location);
  glm_translate(spawn_location, (vec3){500, 200, -200});
  uint32_t new_ship =
      make_entity(spawn_location, mining_faction, ENTITY_COLORS[station_a], 1,
                  (vec3){20, 20, 100}, ship_model, "AI Ship");
  give_gun(new_ship, 1000, 0.2, ship_model, 1);
  entity_set_ondeath(new_ship, ONDEATH_EXPLODE);
  entity_set_health(new_ship, 5);

  // Timings
  float time;

  while (!done) {
    // EVENTS
    float delta_t_f32 = ((float)(SDL_GetTicksNS() - time)) / 1e9f;
    time              = SDL_GetTicksNS();
    SDL_Event event;
    vlk_nk_inputBegin();
    while (SDL_PollEvent(&event)) {
      vlk_nk_handleEvent(&event);
      switch (event.type) {
      case SDL_EVENT_WINDOW_RESIZED: {
        printf("EVENT_RESIZED %d %d\n", event.window.data1, event.window.data2);
        vlk_createSwapchain();
        break;
      }
      case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (nk_window_is_any_hovered(UI_CTX)) {
          continue;
        }

        ui_close_ctx();

        if (event.button.button != SDL_BUTTON_RIGHT)
          break;
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        int winheight, winwidth;
        SDL_GetWindowSize(window, &winwidth, &winheight);
        // TEST Selection
        float x_ndc = (2.0f * mx / winwidth) - 1.0f;
        float y_ndc = 1.0f - (2.0f * my / winheight); // flip Y if needed

        vec4 near_clip = {x_ndc, y_ndc, 0.0f, 1.0f}; // Vulkan zo: 0 = near
        vec4 far_clip  = {x_ndc, y_ndc, 1.0f, 1.0f}; // 1 = far

        mat4 view, invVP;
        glm_mat4_inv(GAME_CAM_TRANSFORM, view);
        glm_mat4_mul(GAME_PERSP_PROJ, view, invVP);
        glm_mat4_inv(invVP, invVP);

        vec4 nw, fw;
        glm_mat4_mulv(invVP, near_clip, nw);
        glm_vec4_scale(nw, 1.0f / nw[3], nw);
        glm_mat4_mulv(invVP, far_clip, fw);
        glm_vec4_scale(fw, 1.0f / fw[3], fw);

        vec3 ray_origin, ray_dir;
        glm_vec3(nw, ray_origin);
        glm_vec3_sub((vec3){fw[0], fw[1], fw[2]}, ray_origin, ray_dir);
        glm_vec3_normalize(ray_dir);

        printf("Origin: %f,%f,%f\n", ray_origin[0], ray_origin[1],
               ray_origin[2]);
        printf("Dir : %f,%f,%f\n", ray_dir[0], ray_dir[1], ray_dir[2]);

        check_intersection(ray_origin, ray_dir);
        if (LAST_RAY_COUNT > 0) {
          printf("Opening context menu\n");
          ui_open_context_menu_for(LAST_RAY_ENTITY_IDS[0]);
        }
        break;
      }
      case SDL_EVENT_QUIT: {
        done = true;
        break;
      }
      case SDL_EVENT_MOUSE_MOTION: {
        if (FREECAM) {
          glm_rotate_y(GAME_CAM_TRANSFORM, event.motion.xrel / 1000.,
                       GAME_CAM_TRANSFORM);
          glm_rotate_x(GAME_CAM_TRANSFORM, event.motion.yrel / 1000.,
                       GAME_CAM_TRANSFORM);
        }
        break;
      }
      }
    }
    vlk_nk_inputEnd();

    if (!nk_window_is_any_hovered(UI_CTX)) {
      if (!FREECAM) {
        // TODO: Get mouse position in -1,1 coords, use to drive ship turn
        int winheight, winwidth;
        SDL_GetWindowSize(window, &winwidth, &winheight);
        float mousex, mousey;
        SDL_GetMouseState(&mousex, &mousey);
        mousex =
            fmin(fmax((((float)mousex / (float)winwidth) - 0.5) * 4, -1.), 1.);
        mousey =
            fmin(fmax((((float)mousey / (float)winheight) - 0.5) * 4, -1.), 1.);
        ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][0] = mousey;
        ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][1] = mousex;
      }
    } else {
      ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][0] = 0;
      ENTITY_CURRENT_VEC_THRUST[PLAYER_CONTROLLED_ENTITY][1] = 0;
    }

    if (ENTITY_DEAD[PLAYER_CONTROLLED_ENTITY] > 2) {
      glm_mat4_identity(spawn_location);
      glm_translate(spawn_location, (vec3){0, 100, 100});

      PLAYER_CONTROLLED_ENTITY =
          make_entity(spawn_location, player_faction, (vec3){0., 1., 0.}, 1,
                      (vec3){20, 20, 100}, ship_model, "Player Ship");
      give_gun(PLAYER_CONTROLLED_ENTITY, 1000, 0.2, ship_model, 1);
      entity_set_ondeath(PLAYER_CONTROLLED_ENTITY, ONDEATH_EXPLODE);
      entity_set_health(PLAYER_CONTROLLED_ENTITY, 5);
    }

    sim_loop(delta_t_f32);

    // GRAPHICS
    vlk_queueModelDrawCommands(ENTITY_COUNT, ENTITY_TRANSFORM, ENTITY_COLORS,
                               ENTITY_SCALE, ENTITY_MODEL, ENTITY_DEAD);
    vlk_queueFxDrawCommands(FX_COUNT, FX_LOCATION, FX_T, FX_MAX_T);

    ui_draw(window);

    // Freecam
    if (FREECAM) {
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
      ENTITY_CURRENT_THRUST[PLAYER_CONTROLLED_ENTITY][2] =
          ENTITY_THRUST_POWER[PLAYER_CONTROLLED_ENTITY][2] *
          (keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]);
      ENTITY_CURRENT_THRUST[PLAYER_CONTROLLED_ENTITY][0] =
          ENTITY_THRUST_POWER[PLAYER_CONTROLLED_ENTITY][0] *
          (keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q]);
      ENTITY_CURRENT_THRUST[PLAYER_CONTROLLED_ENTITY][1] =
          ENTITY_THRUST_POWER[PLAYER_CONTROLLED_ENTITY][1] *
          (keys[SDL_SCANCODE_X] - keys[SDL_SCANCODE_Z]);
      glm_mat4_copy(ENTITY_TRANSFORM[PLAYER_CONTROLLED_ENTITY],
                    GAME_CAM_TRANSFORM);
      glm_translate(GAME_CAM_TRANSFORM, (vec3){0., 4., -10.});

      GUN_ACTIVE[PLAYER_CONTROLLED_ENTITY] = keys[SDL_SCANCODE_SPACE];
    }

    if (vlk_beginDraw() != 0)
      continue;

    vlk_issueDraws();
    vlk_nk_render();
    vlk_endDraw();
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
