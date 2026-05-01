#ifndef GAME_ENGINE
#define GAME_ENGINE
#include "vendor/cglm/box.h"
#define BVH_IMPLEMENTATION
#include "./bvh.h"
#include "./entities.h"
#include "./physics.h"
#include "./ai.h"
#include "vendor/cglm/cglm.h"
#include "vendor/cglm/mat4.h"
#include "vendor/cglm/vec3.h"
#include <math.h>

#define MAX_ENTITIES 2000000
#define MAX_FX 1000

// FX STUFF
vec3 FX_LOCATION[MAX_FX];
float FX_T[MAX_FX];
float FX_MAX_T[MAX_FX];
uint32_t FX_COUNT = 0;

uint32_t make_entity(mat4 transform, uint32_t faction, vec3 col,
                     float vec_thrust, float thrust, uint32_t model_id,
                     char *name) {
  glm_mat4_identity(ENTITY_TRANSFORM[ENTITY_COUNT]);
  glm_mat4_copy(transform, ENTITY_TRANSFORM[ENTITY_COUNT]);
  ENTITY_MODEL[ENTITY_COUNT]                 = model_id;
  ENTITY_FACTION[ENTITY_COUNT]               = faction;
  ENTITY_VEC_THRUST[ENTITY_COUNT]            = vec_thrust;
  ENTITY_THRUST_POWER[ENTITY_COUNT]          = thrust;
  ENTITY_MAX_VEL[ENTITY_COUNT]               = thrust;
  ENTITY_TARGETS[ENTITY_COUNT]               = -1;
  ENTITY_CURRENT_THRUST[ENTITY_COUNT][0]     = 0;
  ENTITY_CURRENT_THRUST[ENTITY_COUNT][1]     = 0;
  ENTITY_CURRENT_THRUST[ENTITY_COUNT][2]     = 0;
  ENTITY_CURRENT_VEC_THRUST[ENTITY_COUNT][0] = 0;
  ENTITY_CURRENT_VEC_THRUST[ENTITY_COUNT][1] = 0;
  ENTITY_CURRENT_VEC_THRUST[ENTITY_COUNT][2] = 0;
  ENTITY_SCALE[ENTITY_COUNT][0]              = 1.;
  ENTITY_SCALE[ENTITY_COUNT][1]              = 1.;
  ENTITY_SCALE[ENTITY_COUNT][2]              = 1.;
  ENTITY_COLLIDER_GROUP[ENTITY_COUNT]        = 1;
  ENTITY_NAME[ENTITY_COUNT]                  = name;
  ENTITY_DEAD[ENTITY_COUNT]                  = 0;
  glm_vec3_copy(col, ENTITY_COLORS[ENTITY_COUNT]);
  glm_vec3_zero(ENTITY_INERTIA[ENTITY_COUNT]);
  return ENTITY_COUNT++;
}

void give_gun(uint32_t ship, float speed, float reload_time, uint32_t model) {
  GUN_MODEL[ship]       = model;
  GUN_SPEED[ship]       = speed;
  GUN_RELOAD_TIME[ship] = reload_time;
  GUN_ACTIVE[ship]      = 0;
}

void play_fx(float delta_time) {
  for (uint32_t t = 0; t < FX_COUNT; t++) {
    if (FX_T[t] < FX_MAX_T[t]) {
      FX_T[t] += delta_time;
    }
  }
}

void spawn_fx(vec3 loc, float duration) {
  FX_T[FX_COUNT]     = 0.;
  FX_MAX_T[FX_COUNT] = duration;
  glm_vec3_copy(loc, FX_LOCATION[FX_COUNT]);
  FX_COUNT++;
  FX_COUNT = FX_COUNT % MAX_FX;
}

int handle_collision(uint32_t prim_index, void *user) {
  uint32_t *checking_entity = (uint32_t *)user;
  uint32_t found_entity     = AABB_IDS[prim_index];
  if (*checking_entity != found_entity) {
    spawn_fx(ENTITY_TRANSFORM[found_entity][3], 1);
    ENTITY_COLORS[found_entity][0] = 0.5;
    ENTITY_COLORS[found_entity][1] = 0.5;
    ENTITY_COLORS[found_entity][2] = 0.5;
    ENTITY_DEAD[found_entity]      = 0.1;
  }
  return 0;
}

void death_animations(float delta_time) {
  for (uint32_t t = 0; t < ENTITY_COUNT; t++) {
    if ((ENTITY_DEAD[t] > 0) & (ENTITY_DEAD[t] < 5)) {
      ENTITY_DEAD[t] += delta_time;
    }
  }
}

void do_collisions() {
  for (uint32_t entity = 0; entity < ENTITY_COUNT; entity++) {
    bvh_aabb_query(&ENTITY_BVH,
                   &(bvh_aabb){
                       .min = {ENTITY_TRANSFORM[entity][3][0],
                               ENTITY_TRANSFORM[entity][3][1],
                               ENTITY_TRANSFORM[entity][3][2]},
                       .max = {ENTITY_TRANSFORM[entity][3][0],
                               ENTITY_TRANSFORM[entity][3][1],
                               ENTITY_TRANSFORM[entity][3][2]},
                   },
                   handle_collision, &entity);
  }
}

void fire_guns(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (GUN_RELOAD_TIME[ship]) {
      if (GUN_RELOAD[ship] > 0) {
        GUN_RELOAD[ship] -= delta_time;
      } else if (GUN_ACTIVE[ship] == 1) {
        GUN_RELOAD[ship] = GUN_RELOAD_TIME[ship];
        uint32_t bullet =
            make_entity(ENTITY_TRANSFORM[ship], -1, (vec3){1., 0., 0.}, 0, 0,
                        GUN_MODEL[ship], "Bullet");
        vec3 inertia = {0, 0, GUN_SPEED[ship]};
        glm_mat4_mulv3(ENTITY_TRANSFORM[ship], inertia, 0., inertia);
        glm_vec3_copy(inertia, ENTITY_INERTIA[bullet]);
        glm_translate(ENTITY_TRANSFORM[bullet],
                      (vec3){0., 0., GUN_SPEED[ship] * 0.01});
        ENTITY_MAX_VEL[bullet]        = 1000.;
        ENTITY_SCALE[bullet][0]       = 0.1;
        ENTITY_SCALE[bullet][1]       = 0.1;
        ENTITY_SCALE[bullet][2]       = GUN_SPEED[ship] * 0.005;
        ENTITY_COLLIDER_GROUP[bullet] = 0;
      }
    }
  }
}

void sim_loop(float delta_time) {

    // AI routines
    ai_select_targets();
    ai_thrusters();
    ai_shoot();

    // Physis
    build_entity_bvh();
    apply_vec_thrusters(delta_time);
    apply_thrusters(delta_time);
    apply_movement(delta_time);
    do_collisions();

    // Guns
    fire_guns(delta_time);
    death_animations(delta_time);

    // FX
    play_fx(delta_time);
}


#endif
