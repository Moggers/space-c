#ifndef GAME_AI
#define GAME_AI

#include "./contracts.h"
#include "./entities.h"
#include "./faction.h"
#include "bvh.h"
#include "map_loader.h"
#include "physics.h"
#include "vendor/cglm/mat4.h"
#include "vendor/cglm/util.h"
#include "vendor/cglm/vec3.h"
#include <string.h>
#include <vulkan/vulkan_core.h>

#define AIM_IDLE 0
#define AIM_KILL 1
#define AIM_COLLECT 2
#define AIM_DOCKING 3
#define AIM_LANDING 3

uint32_t AI_TARGETS[MAX_ENTITIES];
uint32_t AI_MODE[MAX_ENTITIES];
vec3 AI_NAVIGATE_TO[MAX_ENTITIES];
uint32_t AI_ALIGN_TO[MAX_ENTITIES];

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
  uint32_t targId = AABB_IDS[prim_index];
  if (ENTITY_IS_ASTEROID[targId]) {
    if (ENTITY_SCALE[targId][0] <= ENTITY_SCALE[shipId][0]) {
      AI_TARGETS[shipId] = targId;
      return 1;
    }
    if (AI_TARGETS[shipId] == -1 ||
        ENTITY_SCALE[targId][0] < ENTITY_SCALE[AI_TARGETS[shipId]][0]) {
      AI_TARGETS[shipId] = targId;
      return 0;
    }
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
      uint32_t issuer   = CONTRACT_ISSUING_ENTITY[contract];
      switch (CONTRACT_TYPE[contract]) {
      case CONTRACT_ORE: {

        // Do we already have the shit we need
        for (uint32_t k = 0; k < INVENTORY_SLOTS; k++) {
          if (ENTITY_INVENTORY_COUNT[ship][k] <= CONTRACT_AMOUNT[contract] &&
              ENTITY_INVENTORY_ITEMS[ship][k] == ItemOre) {
            AI_TARGETS[ship] = CONTRACT_ISSUING_ENTITY[contract];
            AI_MODE[ship]    = AIM_DOCKING;
            if (ENTITY_DOCKED[ship]) {
              int32_t recipient_slot = -1;
              for (uint32_t rslot = 0; rslot < INVENTORY_SLOTS; rslot++) {
                if (ENTITY_INVENTORY_ITEMS[issuer][rslot] ==
                        ENTITY_INVENTORY_ITEMS[ship][k] ||
                    ENTITY_INVENTORY_COUNT[issuer][rslot] == 0) {
                  recipient_slot = rslot;
                }
              }
              if (recipient_slot > -1) {
                ENTITY_INVENTORY_COUNT[issuer][recipient_slot] +=
                    ENTITY_INVENTORY_COUNT[ship][k];
                ENTITY_INVENTORY_ITEMS[issuer][k] =
                    ENTITY_INVENTORY_ITEMS[ship][k];
                ENTITY_INVENTORY_ITEMS[ship][k] = 0;
                ENTITY_DOCKING[ship][0]         = 0;
                ENTITY_DOCKING[ship][1]         = 0;
                ENTITY_DOCKED[ship]             = 0;
                AI_ALIGN_TO[ship]               = 0;
                AI_MODE[ship]                   = AIM_IDLE;
                break;
              }
            }
            uint32_t model        = ENTITY_MODEL[AI_TARGETS[ship]];
            map_t *map            = MODEL_MAP[model];
            float closestdockdist = 999999;
            uint32_t dockingid;
            for (uint32_t enti = 0; enti < map->entity_count; enti++) {
              map_entity *entity    = &map->entities[enti];
              const char *classname = map_entity_get(entity, "classname");
              if (strcmp(classname, "info_landingpad") == 0) {

                // Find docking location and distance
                vec3 landingpadloc, landingpadnorm;
                map_entity_get_vec3(entity, "origin", landingpadloc);
                map_entity_get_vec3(entity, "normal", landingpadnorm);
                // TODO: This scale should probably be based on the size of the
                // ship
                glm_vec3_scale(landingpadnorm, 5, landingpadnorm);
                glm_vec3_add(landingpadnorm, landingpadloc, landingpadloc);
                glm_mat4_mulv3(ENTITY_TRANSFORM[AI_TARGETS[ship]],
                               landingpadloc, 1, landingpadloc);
                float dockdist =
                    glm_vec3_distance(landingpadloc, ENTITY_TRANSFORM[ship][3]);

                // If this docking point is closer than any previous discovered
                if (dockdist < closestdockdist) {
                  // Go there
                  closestdockdist = dockdist;
                  vec3 heading, controlledpos;
                  glm_vec3_sub(landingpadloc, ENTITY_TRANSFORM[ship][3],
                               heading);
                  glm_vec3_norm(heading);
                  glm_vec3_copy(landingpadloc, AI_NAVIGATE_TO[ship]);
                  dockingid = enti;
                }
              }
            }
            float dist = glm_vec3_distance(ENTITY_TRANSFORM[ship][3],
                                           AI_NAVIGATE_TO[ship]);
            if (dist < 5) {
              AI_ALIGN_TO[ship] = AI_TARGETS[ship];
            }
            if (dist < 3 &&
                glm_vec3_dot(ENTITY_TRANSFORM[ship][2],
                             ENTITY_TRANSFORM[AI_TARGETS[ship]][2]) > 0.8) {
              ENTITY_DOCKING[ship][0] = AI_TARGETS[ship];
              ENTITY_DOCKING[ship][1] = dockingid;
            }
            break;
          }
        }

        if ((AI_MODE[ship] == AIM_DOCKING) | (AI_MODE[ship] == AIM_LANDING)) {
          continue;
        }
        // Go find it
        AI_TARGETS[ship] = -1;
        bvh_closest_query(
            &ENTITY_BVH,
            &(bvh_closest){.point       = {ENTITY_TRANSFORM[ship][3][0],
                                           ENTITY_TRANSFORM[ship][3][1],
                                           ENTITY_TRANSFORM[ship][3][2]},
                           .max_dist_sq = 9999999},
            target_asteroid_cb, &ship);

        if (AI_TARGETS[ship] != -1) {
          uint32_t target = AI_TARGETS[ship];
          // Get target location
          glm_vec3_copy(ENTITY_TRANSFORM[target][3], AI_NAVIGATE_TO[ship]);
          // Add inertia of the target
          vec3 targinertia;
          glm_vec3_copy(ENTITY_INERTIA[target], targinertia);
          glm_vec3_scale(targinertia, 2, targinertia);
          glm_vec3_add(targinertia, AI_NAVIGATE_TO[ship], AI_NAVIGATE_TO[ship]);
          if (ENTITY_SCALE[ship][0] >= ENTITY_SCALE[AI_TARGETS[ship]][0]) {
            ENTITY_COLLECTING[ship] = AI_TARGETS[ship];
            AI_MODE[ship]           = AIM_COLLECT;
          } else {
            AI_MODE[ship]    = AIM_KILL;
            float *targscale = ENTITY_SCALE[AI_TARGETS[ship]];
            float *shipscale = ENTITY_SCALE[ship];
            float maxsize =
                fmax(targscale[0], fmax(targscale[1], targscale[2])) * 2 +
                fmax(shipscale[0], fmax(shipscale[1], shipscale[2])) * 4;
          }
          break;
        }
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
    if (ship == PLAYER_CONTROLLED_ENTITY) {
      continue;
    }
    if (ENTITY_DEAD[ship]) {
      GUN_ACTIVE[ship] = false;
      continue;
    }
    if (AI_MODE[ship] != AIM_KILL) {
      GUN_ACTIVE[ship] = false;
      continue;
    }
    if ((GUN_RELOAD_TIME[ship] != 0) & (AI_TARGETS[ship] != -1)) {

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
    mat4 inv;
    glm_mat4_inv(ENTITY_TRANSFORM[ship], inv);

    // Get target location
    vec3 rel_target_location;

    // Find a correction location
    float gapsize = 0;
    gapsize += glm_vec3_distance(ENTITY_INERTIA[ship],
                                 (vec3){
                                     0,
                                     0,
                                 }) /
               3;
    gapsize += glm_max(ENTITY_SCALE[ship][0],
                       glm_max(ENTITY_SCALE[ship][1], ENTITY_SCALE[ship][2])) * 4;
    find_correction_location(ENTITY_TRANSFORM[ship][3], AI_NAVIGATE_TO[ship],
                             gapsize, rel_target_location);
    glm_vec3_copy(rel_target_location, ENTITY_TRANSFORM[DEBUG_MARKERS[0]][3]);
    glm_vec3_copy(AI_NAVIGATE_TO[ship], ENTITY_TRANSFORM[DEBUG_MARKERS[1]][3]);

    // Get raw target location relative to ship coordinates normalized
    vec3 target_heading;
    if (AI_ALIGN_TO[ship]) {
      // Fixing aligngment
      glm_vec3_copy(ENTITY_TRANSFORM[AI_TARGETS[ship]][2], target_heading);
      glm_vec3_rotate_m4(inv, target_heading, target_heading);
    } else {
      // Aim at target
      glm_mat4_mulv3(inv, rel_target_location, 1, target_heading);
      glm_vec3_normalize(target_heading);
    }
    ENTITY_CURRENT_VEC_THRUST[ship][0] =
        glm_clamp(-target_heading[1] * 5, -ENTITY_VEC_THRUST[ship],
                  ENTITY_VEC_THRUST[ship]);
    ENTITY_CURRENT_VEC_THRUST[ship][1] =
        glm_clamp(target_heading[0] * 5, -ENTITY_VEC_THRUST[ship],
                  ENTITY_VEC_THRUST[ship]);
    ENTITY_CURRENT_VEC_THRUST[ship][2] =
        glm_clamp(target_heading[0] * 5, -ENTITY_VEC_THRUST[ship],
                  ENTITY_VEC_THRUST[ship]);

    // Add inverse of velocity (dampening)
    vec3 delta_v;
    glm_vec3_sub((vec3){0., 0., 0.}, ENTITY_INERTIA[ship], delta_v);
    glm_vec3_scale(delta_v, 2, delta_v);
    glm_vec3_add(rel_target_location, delta_v, rel_target_location);
    // Transform target location into local coordinates
    glm_mat4_mulv3(inv, rel_target_location, 1, rel_target_location);

#define BREAKING_DISTANCE 50
    // Power corresponds to more than BREAKING_DISTANCE units along the thrust
    // axis. Get the throttle position on each axis
    float rpwr =
        glm_clamp(glm_vec3_dot((vec3){1., 0., .0}, rel_target_location) /
                      BREAKING_DISTANCE,
                  -1, 1);
    float upwr = glm_clamp(glm_vec3_dot((vec3){0, 1, 0}, rel_target_location) /
                               BREAKING_DISTANCE,
                           -1, 1);
    float fpwr =
        glm_clamp(glm_vec3_dot((vec3){0., 0., 1}, rel_target_location) /
                      BREAKING_DISTANCE,
                  -1, 1.);
    // Apply throttle, multiplied by actual engine power along all three axes
    glm_vec3_mul((vec3){rpwr, upwr, fpwr}, ENTITY_THRUST_POWER[ship],
                 ENTITY_CURRENT_THRUST[ship]);
  }
}

#endif
