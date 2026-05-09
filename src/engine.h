#ifndef GAME_ENGINE
#define GAME_ENGINE
#include "./ai.h"
#include "./bvh.h"
#include "./entities.h"
#include "./physics.h"
#include "vendor/cglm/mat4.h"
#include "vendor/cglm/vec3.h"
#include <math.h>
#include <stdlib.h>

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
  AI_TARGETS[ENTITY_COUNT]               = -1;
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
  memset(ENTITY_CONTRACTS[ENTITY_COUNT], 0, 32 * sizeof(uint32_t));
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

void ondeath_explode(uint32_t entityId) {
  spawn_fx(ENTITY_TRANSFORM[entityId][3], 1);
  ENTITY_COLORS[entityId][0] = 0.5;
  ENTITY_COLORS[entityId][1] = 0.5;
  ENTITY_COLORS[entityId][2] = 0.5;
  ENTITY_DEAD[entityId]      = 0.1;
}

void ondeath_delete(uint32_t entityId) {
  spawn_fx(ENTITY_TRANSFORM[entityId][3], 1);
  ENTITY_DEAD[entityId]           = 0.1;
  ENTITY_MODEL[entityId]          = 0;
  ENTITY_COLLIDER_GROUP[entityId] = 0;
  glm_mat4_identity(ENTITY_TRANSFORM[entityId]);
}

void ondeath_split(uint32_t entityId) {
  printf("Splitting\n");
  double randdir = rand() * 10;
  if (ENTITY_MAXHEALTH[entityId] == 1) {
    ondeath_delete(entityId);
    return;
  }
  vec3 randoffset = {sin(randdir) / 2, cos(randdir) / 2, 0};
  float size =
      ENTITY_SCALE[entityId][0] * (MODEL_HULLS[ENTITY_MODEL[entityId]].max[0] -
                                   MODEL_HULLS[ENTITY_MODEL[entityId]].min[0]);
  glm_vec3_scale(randoffset, size, randoffset);
  ENTITY_SCALE[entityId][0] /= 2;
  ENTITY_SCALE[entityId][1] /= 2;
  ENTITY_SCALE[entityId][2] /= 2;
  ENTITY_MAXHEALTH[entityId] /= 2;
  ENTITY_HEALTH[entityId] = ENTITY_MAXHEALTH[entityId];
  uint32_t firstNew = entity_copy(entityId);
  ENTITY_TRANSFORM[entityId][3][0] -= randoffset[0];
  ENTITY_TRANSFORM[entityId][3][1] -= randoffset[1];
  ENTITY_TRANSFORM[entityId][3][2] -= randoffset[2];
  ENTITY_TRANSFORM[firstNew][3][0] += randoffset[0];
  ENTITY_TRANSFORM[firstNew][3][1] += randoffset[1];
  ENTITY_TRANSFORM[firstNew][3][2] += randoffset[2];
}

int handle_collision(uint32_t prim_index_a, uint32_t prim_index_b, void *user) {
  uint32_t entity_a = AABB_IDS[prim_index_a];
  uint32_t entity_b = AABB_IDS[prim_index_b];
  if (entity_b != entity_a) {
    if (ENTITY_HEALTH[entity_b] <= 0 || ENTITY_HEALTH[entity_a] <= 0) {
      return 0;
    }
    uint32_t ahealth = ENTITY_HEALTH[entity_b];
    uint32_t bhealth = ENTITY_HEALTH[entity_a];
    ENTITY_HEALTH[entity_b] -= bhealth;
    ENTITY_HEALTH[entity_a] -= ahealth;

    if (ENTITY_HEALTH[entity_b] <= 0) {
      switch (ENTITY_ONDEATH[entity_b]) {
      case ONDEATH_EXPLODE: {
        ondeath_explode(entity_b);
        break;
      }
      case ONDEATH_SPLIT: {
        ondeath_split(entity_b);
        break;
      }
      case ONDEATH_DELETE: {
        ondeath_delete(entity_b);
        break;
      }
      }
    }
    if (ENTITY_HEALTH[entity_a] <= 0) {
      switch (ENTITY_ONDEATH[entity_a]) {
      case ONDEATH_EXPLODE: {
        ondeath_explode(entity_a);
        break;
      }
      case ONDEATH_SPLIT: {
        ondeath_split(entity_a);
        break;
      }
      case ONDEATH_DELETE: {
        ondeath_delete(entity_a);
        break;
      }
      }
    }
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

uint32_t LAST_RAY_ENTITY_IDS[32];
uint32_t LAST_RAY_COUNT = 0;
int ray_cb(uint32_t prim_index, bvh_ray *ray, void *user) {
  uint32_t eid                        = AABB_IDS[prim_index];
  LAST_RAY_ENTITY_IDS[LAST_RAY_COUNT] = eid;
  return ++LAST_RAY_COUNT < 32;
}

void check_intersection(vec3 start, vec3 dir) {

  bvh_ray ray;
  bvh_ray_init(&ray, start, dir, 0, 100000);
  LAST_RAY_COUNT = 0;
  bvh_ray_query_tight(&ENTITY_BVH, &ray, &ray_cb, 0);
}

void do_collisions() { bvh_self_overlap(&ENTITY_BVH, handle_collision, 0); }

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
        ENTITY_COLLIDER_GROUP[bullet] = 1;
        entity_set_ondeath(bullet, ONDEATH_DELETE);
        entity_set_health(bullet, 1);
      }
    }
  }
}

void sim_loop(float delta_time) {

  // Faction routines
  ai_accept_contracts();

  // AI routines
  // ai_ship_select_target();
  ai_ship_select_tasks();
  ai_ship_thrusters();
  ai_ship_shoot();

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
