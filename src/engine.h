#ifndef GAME_ENGINE
#define GAME_ENGINE
#include "vendor/cglm/cglm.h"
#include "vendor/cglm/mat4.h"
#include "vendor/cglm/vec3.h"
#include <math.h>

#define MAX_ENTITIES 2000000
#define MAX_FX 1000

typedef struct Gun {
  float reload_time;
  float reload;
  float speed;
  bool active;
  uint32_t model;
} Gun;

// ETNTIY STUFF
int32_t ENTITY_COUNT;
mat4 ENTITY_TRANSFORM[MAX_ENTITIES];
int32_t ENTITY_MODEL[MAX_ENTITIES];
int32_t ENTITY_FACTION[MAX_ENTITIES];
vec3 ENTITY_COLORS[MAX_ENTITIES];
vec3 ENTITY_INERTIA[MAX_ENTITIES];
uint32_t ENTITY_TARGETS[MAX_ENTITIES];
float ENTITY_VEC_THRUST[MAX_ENTITIES];
float ENTITY_THRUST_POWER[MAX_ENTITIES];
float ENTITY_CURRENT_THRUST[MAX_ENTITIES];
float ENTITY_MAX_VEL[MAX_ENTITIES];
vec3 ENTITY_CURRENT_VEC_THRUST[MAX_ENTITIES];
uint32_t PLAYER_CONTROLLED_ENTITY;
vec3 ENTITY_SCALE[MAX_ENTITIES];
Gun ENTITY_GUN[MAX_ENTITIES];
uint32_t ENTITY_COLLIDER_GROUP[MAX_ENTITIES];
char *ENTITY_NAME[MAX_ENTITIES];

// FIX STUFF
vec3 FX_LOCATION[MAX_FX];
float FX_T[MAX_FX];
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
  ENTITY_CURRENT_THRUST[ENTITY_COUNT]        = 0;
  ENTITY_CURRENT_VEC_THRUST[ENTITY_COUNT][0] = 0;
  ENTITY_CURRENT_VEC_THRUST[ENTITY_COUNT][1] = 0;
  ENTITY_CURRENT_VEC_THRUST[ENTITY_COUNT][2] = 0;
  ENTITY_SCALE[ENTITY_COUNT][0]              = 1.;
  ENTITY_SCALE[ENTITY_COUNT][1]              = 1.;
  ENTITY_SCALE[ENTITY_COUNT][2]              = 1.;
  ENTITY_COLLIDER_GROUP[ENTITY_COUNT]        = 1;
  ENTITY_NAME[ENTITY_COUNT]                  = name;
  glm_vec3_copy(col, ENTITY_COLORS[ENTITY_COUNT]);
  glm_vec3_zero(ENTITY_INERTIA[ENTITY_COUNT]);
  return ENTITY_COUNT++;
}

void ai_select_targets() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_TARGETS[ship] == -1) {
      float distance = -1;
      for (uint32_t potential_target = 0; potential_target < ENTITY_COUNT;
           potential_target++) {
        if (ENTITY_FACTION[potential_target] > -1 &&
            ENTITY_FACTION[ship] != ENTITY_FACTION[potential_target] &&
            ENTITY_MODEL[potential_target] == 0) {
          float new_dist = glm_vec3_distance(
              ENTITY_TRANSFORM[ship][3], ENTITY_TRANSFORM[potential_target][3]);
          if (distance == -1 || (new_dist < distance)) {
            distance             = new_dist;
            ENTITY_TARGETS[ship] = potential_target;
          }
        }
      }
    }
  }
}

void ai_shoot() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_GUN[ship].reload_time != 0 && ship != PLAYER_CONTROLLED_ENTITY) {
      mat4 inv;
      glm_mat4_inv(ENTITY_TRANSFORM[ship], inv);
      vec3 reltarg;
      glm_mat4_mulv3(inv, ENTITY_TRANSFORM[ENTITY_TARGETS[ship]][3], 1.,
                     reltarg);
      ENTITY_GUN[ship].active =
          fabs(reltarg[1]) < 50 && fabs(reltarg[0]) < 50 && (reltarg[2] > 0);
    }
  }
}

void give_gun(uint32_t ship, float speed, float reload_time, uint32_t model) {
  ENTITY_GUN[ship].model       = model;
  ENTITY_GUN[ship].speed       = speed;
  ENTITY_GUN[ship].reload_time = reload_time;
  ENTITY_GUN[ship].active      = 0;
}

void play_fx(float delta_time) {
  for (uint32_t t = 0; t < FX_COUNT; t++) {
    FX_T[t] += delta_time;
  }
}

void spawn_fx(vec3 loc) {
  FX_T[FX_COUNT] = 0.;
  glm_vec3_copy(loc, FX_LOCATION[FX_COUNT]);
  FX_COUNT++;
  FX_COUNT = FX_COUNT % MAX_FX;
}

void do_collisions() {
  for (uint32_t entity = 0; entity < ENTITY_COUNT; entity++) {
    for (uint32_t collider = 0; collider < ENTITY_COUNT; collider++) {
      if ((collider == entity) | !ENTITY_COLLIDER_GROUP[collider] |
          !ENTITY_COLLIDER_GROUP[entity] |
          (ENTITY_COLLIDER_GROUP[entity] == ENTITY_COLLIDER_GROUP[collider])) {
        continue;
      }
      if ((fabs(ENTITY_TRANSFORM[entity][3][0] -
                ENTITY_TRANSFORM[collider][3][0]) +
           fabs(ENTITY_TRANSFORM[entity][3][1] -
                ENTITY_TRANSFORM[collider][3][1]) +
           fabs(ENTITY_TRANSFORM[entity][3][2] -
                ENTITY_TRANSFORM[collider][3][2])) < 5) {
        printf("%s colliding with %s\n", ENTITY_NAME[entity],
               ENTITY_NAME[collider]);
        spawn_fx(ENTITY_TRANSFORM[entity][3]);
      }
    }
  }
}

void fire_guns(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_GUN[ship].reload_time) {
      if (ENTITY_GUN[ship].reload >= 0) {
        ENTITY_GUN[ship].reload -= delta_time;
      } else if (ENTITY_GUN[ship].active == 1) {
        ENTITY_GUN[ship].reload = ENTITY_GUN[ship].reload_time;
        uint32_t bullet =
            make_entity(ENTITY_TRANSFORM[ship], -1, (vec3){1., 0., 0.}, 0, 0,
                        ENTITY_GUN[ship].model, "Bullet");
        vec3 inertia = {0, 0, ENTITY_GUN[ship].speed};
        glm_mat4_mulv3(ENTITY_TRANSFORM[ship], inertia, 0., inertia);
        glm_vec3_copy(inertia, ENTITY_INERTIA[bullet]);
        glm_translate(ENTITY_TRANSFORM[bullet],
                      (vec3){0., 0., ENTITY_GUN[ship].speed * 0.01});
        ENTITY_MAX_VEL[bullet]        = 1000.;
        ENTITY_SCALE[bullet][0]       = 0.1;
        ENTITY_SCALE[bullet][1]       = 0.1;
        ENTITY_SCALE[bullet][2]       = ENTITY_GUN[ship].speed * 0.005;
        ENTITY_COLLIDER_GROUP[bullet] = 2;
      }
    }
  }
}

void ai_thrusters() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (PLAYER_CONTROLLED_ENTITY == ship) {
      continue;
    }
    if (ENTITY_VEC_THRUST[ship] == -1 || ENTITY_TARGETS[ship] == -1) {
      continue;
    }
    uint32_t target = ENTITY_TARGETS[ship];
    // Turning towards target
    vec3 targ_loc_transformed;
    mat4 inv;
    glm_mat4_inv(ENTITY_TRANSFORM[ship], inv);
    glm_mat4_mulv3(inv, ENTITY_TRANSFORM[target][3], 1, targ_loc_transformed);
    // Pitch = Y/Z (add a bit of Z to avoid divide by 0)
    ENTITY_CURRENT_VEC_THRUST[ship][0] =
        -targ_loc_transformed[1] * (fabs(targ_loc_transformed[2]) + 1);
    // Yaw = 0
    ENTITY_CURRENT_VEC_THRUST[ship][1] = 0.;
    // Roll = X/Y
    ENTITY_CURRENT_VEC_THRUST[ship][2] =
        targ_loc_transformed[0] / -targ_loc_transformed[1];
    ENTITY_CURRENT_THRUST[ship] = ENTITY_THRUST_POWER[ship];
  }
}

void apply_vec_thrusters(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_VEC_THRUST[ship] == -1) {
      continue;
    }
    float thr   = ENTITY_VEC_THRUST[ship];
    float pitch = ENTITY_CURRENT_VEC_THRUST[ship][0];
    float yaw   = ENTITY_CURRENT_VEC_THRUST[ship][1];
    float roll  = ENTITY_CURRENT_VEC_THRUST[ship][2];
    glm_rotate_x(ENTITY_TRANSFORM[ship],
                 fmin(fmax(pitch, -thr), thr) * delta_time,
                 ENTITY_TRANSFORM[ship]);
    glm_rotate_y(ENTITY_TRANSFORM[ship],
                 fmin(fmax(yaw, -thr), thr) * delta_time,
                 ENTITY_TRANSFORM[ship]);
    glm_rotate_z(ENTITY_TRANSFORM[ship],
                 fmin(fmax(roll, -thr), thr) * delta_time,
                 ENTITY_TRANSFORM[ship]);
  }
}

void apply_thrusters(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_THRUST_POWER[ship] == -1 || ENTITY_TARGETS[ship] == -1) {
      continue;
    }

    vec3 heading = {
        ENTITY_TRANSFORM[ship][2][0] * ENTITY_CURRENT_THRUST[ship] * delta_time,
        ENTITY_TRANSFORM[ship][2][1] * ENTITY_CURRENT_THRUST[ship] * delta_time,
        ENTITY_TRANSFORM[ship][2][2] * ENTITY_CURRENT_THRUST[ship] * delta_time,
    };

    glm_vec3_add(ENTITY_INERTIA[ship], heading, ENTITY_INERTIA[ship]);
  }
}

void apply_movement(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    float len = glm_vec3_distance(ENTITY_INERTIA[ship], (vec3){0., 0., 0.});
    len       = fmin(len, ENTITY_MAX_VEL[ship]);
    glm_vec3_normalize(ENTITY_INERTIA[ship]);
    glm_vec3_mul(ENTITY_INERTIA[ship], (vec3){len, len, len},
                 ENTITY_INERTIA[ship]);
    ENTITY_TRANSFORM[ship][3][0] += ENTITY_INERTIA[ship][0] * delta_time;
    ENTITY_TRANSFORM[ship][3][1] += ENTITY_INERTIA[ship][1] * delta_time;
    ENTITY_TRANSFORM[ship][3][2] += ENTITY_INERTIA[ship][2] * delta_time;
  }
}
#endif
