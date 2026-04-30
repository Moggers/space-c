#ifndef GAME_AI
#define GAME_AI

#include "./entities.h"

void ai_select_targets() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_TARGETS[ship] == -1) {
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
            distance             = new_dist;
            ENTITY_TARGETS[ship] = potential_target;
          }
        }
      }
    } else {
      if (ENTITY_DEAD[ENTITY_TARGETS[ship]]) {
        ENTITY_TARGETS[ship] = -1;
      }
    }
  }
}

void ai_shoot() {
  for (uint32_t ship = 0; ship < ENTITY_COUNT; ship++) {
    if (ENTITY_DEAD[ship]) {
      GUN_ACTIVE[ship] = false;
      continue;
    }
    if ((GUN_RELOAD_TIME[ship] != 0) & (ship != PLAYER_CONTROLLED_ENTITY) &
        (ENTITY_TARGETS[ship] != -1)) {
      mat4 inv;
      glm_mat4_inv(ENTITY_TRANSFORM[ship], inv);
      vec3 reltarg;
      glm_mat4_mulv3(inv, ENTITY_TRANSFORM[ENTITY_TARGETS[ship]][3], 1.,
                     reltarg);
      GUN_ACTIVE[ship] =
          fabs(reltarg[1]) < 50 && fabs(reltarg[0]) < 50 && (reltarg[2] > 0);
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
    if (ENTITY_DEAD[ship]) {
      ENTITY_CURRENT_VEC_THRUST[ship][0] = 0.;
      ENTITY_CURRENT_VEC_THRUST[ship][1] = 0.;
      ENTITY_CURRENT_VEC_THRUST[ship][2] = 0.;
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
    ENTITY_CURRENT_THRUST[ship][2] = ENTITY_THRUST_POWER[ship];
  }
}

#endif
