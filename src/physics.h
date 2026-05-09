#ifndef GAME_PHYSICS
#define GAME_PHYSICS
#include "./bvh.h"
#include "./entities.h"
#include "./vendor/fast_obj.h"
#include "vendor/cglm/cglm.h"
#include <string.h>

// COLLECTIONS
bvh_t ENTITY_BVH = {0};
uint32_t AABB_IDS[MAX_ENTITIES];
uint32_t BVH_COUNT;

// HULL STUFF
bvh_aabb MODEL_HULLS[512];

void model_set_hull(uint32_t model_id, fastObjMesh *mesh) {
  bvh_aabb hull = {};
  for (uint32_t t = 0; t < mesh->position_count; t++) {
    float *pos = &mesh->positions[t];
    if (pos[0] < hull.min[0]) {
      hull.min[0] = pos[0];
    }
    if (pos[1] < hull.min[1]) {
      hull.min[1] = pos[1];
    }
    if (pos[2] < hull.min[2]) {
      hull.min[2] = pos[2];
    }
    if (pos[0] > hull.max[0]) {
      hull.max[0] = pos[0];
    }
    if (pos[1] > hull.max[1]) {
      hull.max[1] = pos[1];
    }
    if (pos[2] > hull.max[2]) {
      hull.max[2] = pos[2];
    }
  }
  MODEL_HULLS[model_id] = hull;
}

void build_entity_bvh() {
  uint32_t collider_count = 0;
  for (uint32_t i = 0; i < ENTITY_COUNT; i++) {
    if ((ENTITY_COLLIDER_GROUP[i] != 0) & !ENTITY_DEAD[i]) {
      collider_count++;
    }
  }
  uint32_t i, collider_id;
  bvh_aabb prims[collider_count];
  collider_id = 0;
  for (i = 0; i < ENTITY_COUNT; i++) {
    if ((ENTITY_COLLIDER_GROUP[i] != 0) & !ENTITY_DEAD[i]) {
      memcpy(&prims[collider_id], &MODEL_HULLS[ENTITY_MODEL[i]],
             sizeof(bvh_aabb));
      prims[collider_id].min[0] *= ENTITY_SCALE[i][0];
      prims[collider_id].min[1] *= ENTITY_SCALE[i][1];
      prims[collider_id].min[2] *= ENTITY_SCALE[i][2];
      prims[collider_id].max[0] *= ENTITY_SCALE[i][0];
      prims[collider_id].max[1] *= ENTITY_SCALE[i][1];
      prims[collider_id].max[2] *= ENTITY_SCALE[i][2];

      prims[collider_id].min[0] += ENTITY_TRANSFORM[i][3][0];
      prims[collider_id].min[1] += ENTITY_TRANSFORM[i][3][1];
      prims[collider_id].min[2] += ENTITY_TRANSFORM[i][3][2];
      prims[collider_id].max[0] += ENTITY_TRANSFORM[i][3][0];
      prims[collider_id].max[1] += ENTITY_TRANSFORM[i][3][1];
      prims[collider_id].max[2] += ENTITY_TRANSFORM[i][3][2];

      AABB_IDS[collider_id] = i;

      collider_id++;
    }
  }

  if (collider_count == BVH_COUNT) {
    bvh_refit(&ENTITY_BVH, prims);
  } else {
    bvh_build(&ENTITY_BVH, prims, collider_count, 0);
  }
  BVH_COUNT = collider_count;
}

void apply_vec_thrusters(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if ((ENTITY_VEC_THRUST[ship] == -1) | (ENTITY_DEAD[ship] > 0)) {
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
    if ((ENTITY_DEAD[ship] > 0) | (ENTITY_THRUST_POWER[ship] == -1)) {
      continue;
    }

    vec3 heading = {ENTITY_TRANSFORM[ship][0][0] *
                            ENTITY_CURRENT_THRUST[ship][0] * delta_time +
                        ENTITY_TRANSFORM[ship][1][0] *
                            ENTITY_CURRENT_THRUST[ship][1] * delta_time +
                        ENTITY_TRANSFORM[ship][2][0] *
                            ENTITY_CURRENT_THRUST[ship][2] * delta_time,
                    ENTITY_TRANSFORM[ship][0][1] *
                            ENTITY_CURRENT_THRUST[ship][0] * delta_time +
                        ENTITY_TRANSFORM[ship][1][1] *
                            ENTITY_CURRENT_THRUST[ship][1] * delta_time +
                        ENTITY_TRANSFORM[ship][2][1] *
                            ENTITY_CURRENT_THRUST[ship][2] * delta_time,
                    ENTITY_TRANSFORM[ship][0][2] *
                            ENTITY_CURRENT_THRUST[ship][0] * delta_time +
                        ENTITY_TRANSFORM[ship][1][2] *
                            ENTITY_CURRENT_THRUST[ship][1] * delta_time +
                        ENTITY_TRANSFORM[ship][2][2] *
                            ENTITY_CURRENT_THRUST[ship][2] * delta_time};

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
