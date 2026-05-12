#ifndef GAME_PHYSICS
#define GAME_PHYSICS
#include "./bvh.h"
#include "./entities.h"
#include "./map_loader.h"
#include "vendor/cglm/cglm.h"
#include "vendor/cglm/vec3.h"
#include <string.h>

#define MAX_MODELS 512

// COLLECTIONS
bvh_t ENTITY_BVH = {0};
uint32_t AABB_IDS[MAX_ENTITIES];
uint32_t BVH_COUNT;

// HULL STUFF
bvh_t MODEL_HULLS[MAX_MODELS];
map_t* MODEL_MAP[MAX_MODELS];

void model_set_bvh(uint32_t model_id, map_t *m) {
  uint32_t brush_count = 0;
  for (uint32_t t = 0; t < m->entity_count; t++) {
    brush_count += m->entities[t].brush_count;
  }
  bvh_aabb hulls[brush_count] = {};
  uint32_t chull              = 0;
  for (uint32_t t = 0; t < m->entity_count; t++) {
    for (uint32_t i = 0; i < m->entities[t].brush_count; i++) {
      map_brush_aabb(&m->entities[t].brushes[i], hulls[chull].min,
                     hulls[chull].max);
      chull++;
    }
  }
  bvh_build(&MODEL_HULLS[model_id], hulls, brush_count, 0);
  MODEL_MAP[model_id] = m;
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

      memcpy(&prims[collider_id],
             bvh_root_bounds(&MODEL_HULLS[ENTITY_MODEL[i]]), sizeof(bvh_aabb));
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
    if ((ENTITY_DEAD[ship] > 0) | (ENTITY_THRUST_POWER[ship][0] == -1) | (ENTITY_DOCKING[ship])) {
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

    glm_vec3_copy(ENTITY_INERTIA[ship], ENTITY_LAST_INERTIA[ship]);
    glm_vec3_add(ENTITY_INERTIA[ship], heading, ENTITY_INERTIA[ship]);
    float newheadinglength =
        glm_vec3_distance(ENTITY_INERTIA[ship], (vec3){0., 0., 0.});
    float maxlen =
        glm_vec3_distance(ENTITY_THRUST_POWER[ship], (vec3){0., 0., 0.});
    glm_vec3_normalize(ENTITY_INERTIA[ship]);
    glm_vec3_scale(ENTITY_INERTIA[ship], fmin(maxlen, newheadinglength),
                   ENTITY_INERTIA[ship]);
  }
}

void apply_movement(float delta_time) {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_DOCKING[ship]) {
      continue;
    }
    ENTITY_TRANSFORM[ship][3][0] += ENTITY_INERTIA[ship][0] * delta_time;
    ENTITY_TRANSFORM[ship][3][1] += ENTITY_INERTIA[ship][1] * delta_time;
    ENTITY_TRANSFORM[ship][3][2] += ENTITY_INERTIA[ship][2] * delta_time;
  }
}

#endif
