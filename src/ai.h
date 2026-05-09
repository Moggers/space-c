#ifndef GAME_AI
#define GAME_AI

#include "./contracts.h"
#include "./entities.h"
#include "./faction.h"
#include "bvh.h"
#include "physics.h"
#include "vendor/cglm/vec3.h"
#include <string.h>
#include <vulkan/vulkan_core.h>

#define AIM_KILL 1
#define AIM_COLLECT 2

float AI_MAINTAIN_DIST[MAX_ENTITIES];
uint32_t AI_TARGETS[MAX_ENTITIES];
uint32_t AI_MODE[MAX_ENTITIES];

int ai_contract_suitable(uint32_t entity, uint32_t contract) {

  // If the contract is for ore
  if (CONTRACT_TYPE[contract] == CONTRACT_ORE) {

    // Ship needs a gun to break asteroids
    if (GUN_RELOAD_TIME[entity] != 0) {
      return true;
    }
  }
  return false;
}

void ai_accept_contracts() {
  for (uint32_t entity_id = 0; entity_id < ENTITY_COUNT; entity_id++) {

    // Only run AI on AI ships
    if ((PLAYER_CONTROLLED_ENTITY == entity_id) |
        (ENTITY_FACTION[entity_id] == 0)) {
      continue;
    }

    // Ship must not already be acting on a contract
    if (ENTITY_ACCEPTED_CONTRACT[entity_id] != 0) {
      continue;
    }
    for (uint32_t contract_id = 1; contract_id < CONTRACT_COUNT;
         contract_id++) {

      // Cant accept own contract
      if (CONTRACT_ISSUING_ENTITY[contract_id] == entity_id) {
        continue;
      }

      // Contract must not be claimed
      if (CONTRACT_CLAIMING_ENTITYID[contract_id] != 0) {
        continue;
      }

      // Must be able to fulfil contract
      if (!ai_contract_suitable(entity_id, contract_id)) {
        continue;
      }

      CONTRACT_CLAIMING_ENTITYID[contract_id] = entity_id;
      ENTITY_ACCEPTED_CONTRACT[entity_id]     = contract_id;
    }
  }
}

int target_asteroid_cb(uint32_t prim_index, bvh_closest *closest, void *user) {
  uint32_t shipId = *(uint32_t *)user;
  printf("Found target %s\n", ENTITY_NAME[AABB_IDS[prim_index]]);
  if (ENTITY_IS_ASTEROID[AABB_IDS[prim_index]]) {
    printf("Found asteroid\n");
    AI_TARGETS[shipId] = AABB_IDS[prim_index];
    return 1;
  }
  return 0;
}

void ai_ship_select_tasks() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if ((PLAYER_CONTROLLED_ENTITY == ship) | (ENTITY_FACTION[ship] == -1)) {
      continue;
    }
    if (ENTITY_ACCEPTED_CONTRACT[ship]) {
      uint32_t contract = ENTITY_ACCEPTED_CONTRACT[ship];
      switch (CONTRACT_TYPE[contract]) {
      case CONTRACT_ORE: {
        if (AI_TARGETS[ship] == -1) {
          bvh_closest_query(
              &ENTITY_BVH,
              &(bvh_closest){.point       = {ENTITY_TRANSFORM[ship][3][0],
                                             ENTITY_TRANSFORM[ship][3][1],
                                             ENTITY_TRANSFORM[ship][3][2]},
                             .max_dist_sq = 9999999},
              target_asteroid_cb, &ship);
        }
        if (AI_TARGETS[ship] != -1) {
          if (ENTITY_SCALE[ship][0] >= ENTITY_SCALE[AI_TARGETS[ship]][0]) {
            AI_MODE[ship]          = AIM_COLLECT;
            AI_MAINTAIN_DIST[ship] = ENTITY_SCALE[ship][0];
          } else {
            AI_MODE[ship]    = AIM_KILL;
            float *targscale = ENTITY_SCALE[AI_TARGETS[ship]];
            float maxsize =
                fmax(targscale[0], fmax(targscale[1], targscale[2]));
            AI_MAINTAIN_DIST[ship] = maxsize * 2;
          }
        }
        break;
      }
      }
    }
  }
}

void ai_ship_select_target() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (AI_TARGETS[ship] == -1) {
      float distance = -1;
      for (uint32_t potential_target = 0; potential_target < ENTITY_COUNT;
           potential_target++) {
        if ((ENTITY_FACTION[potential_target] > -1) &
            !ENTITY_DEAD[potential_target] &
            (ENTITY_FACTION[ship] != ENTITY_FACTION[potential_target]) &
            (ENTITY_MODEL[potential_target] == 0)) {
          float new_dist = glm_vec3_distance(
              ENTITY_TRANSFORM[ship][3], ENTITY_TRANSFORM[potential_target][3]);
          if (distance == -1 || (new_dist < distance)) {
            distance         = new_dist;
            AI_TARGETS[ship] = potential_target;
          }
        }
      }
    } else {
      if (ENTITY_DEAD[AI_TARGETS[ship]]) {
        AI_TARGETS[ship] = -1;
      }
    }
  }
}

int ai_ship_ray_shoot_cb(uint32_t primid, bvh_ray *r, void *userdata) {
  uint32_t *ship = userdata;
  if (*ship == AABB_IDS[primid]) {
    return 0;
  }
  GUN_ACTIVE[*ship] = AABB_IDS[primid] == AI_TARGETS[*ship];
  return 1;
}

void ai_ship_shoot() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_DEAD[ship]) {
      GUN_ACTIVE[ship] = false;
      continue;
    }
    if (AI_MODE[ship] != AIM_KILL) {
      GUN_ACTIVE[ship] = false;
      continue;
    }
    if ((GUN_RELOAD_TIME[ship] != 0) & (ship != PLAYER_CONTROLLED_ENTITY) &
        (AI_TARGETS[ship] != -1)) {

      bvh_ray r;
      vec3 targloc;
      glm_vec3_copy(ENTITY_TRANSFORM[ship][2], targloc);
      bvh_ray_init(&r, ENTITY_TRANSFORM[ship][3], targloc, 0, 100000);

      bvh_ray_query_tight(&ENTITY_BVH, &r, ai_ship_ray_shoot_cb, &ship);
    }
  }
}

void ai_ship_thrusters() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (PLAYER_CONTROLLED_ENTITY == ship) {
      continue;
    }
    if (ENTITY_VEC_THRUST[ship] == -1 || AI_TARGETS[ship] == -1) {
      continue;
    }
    if (ENTITY_DEAD[ship]) {
      ENTITY_CURRENT_VEC_THRUST[ship][0] = 0.;
      ENTITY_CURRENT_VEC_THRUST[ship][1] = 0.;
      ENTITY_CURRENT_VEC_THRUST[ship][2] = 0.;
      continue;
    }
    uint32_t target = AI_TARGETS[ship];
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

    // Main thruster
    vec3 modifedloc;
    glm_vec3_copy(ENTITY_TRANSFORM[ship][3], modifedloc);
    glm_vec3_add(modifedloc, ENTITY_INERTIA[ship], modifedloc);
    float cdist = glm_vec3_distance(modifedloc, ENTITY_TRANSFORM[target][3]);

    vec3 relpos;
    glm_vec3_sub(ENTITY_TRANSFORM[AI_TARGETS[ship]][3],
                 ENTITY_TRANSFORM[ship][3], relpos);
    glm_vec3_normalize(relpos);
    float thingo = glm_vec3_dot(relpos, ENTITY_TRANSFORM[ship][2]);

    printf("Dot %f\n", thingo);

    // Above zero if too far, below zero if too close
    float degree = cdist - AI_MAINTAIN_DIST[ship];
    float thrust = fmax(fmin(degree, fabs(ENTITY_THRUST_POWER[ship])),
                        -fabs(ENTITY_THRUST_POWER[ship])) *
                   thingo;
    printf("Thrust power %f\n", thrust);

    ENTITY_CURRENT_THRUST[ship][2] = thrust;
  }
}

#endif
