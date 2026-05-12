#ifndef GAME_ENGINE
#define GAME_ENGINE
#include "./ai.h"
#include "./bvh.h"
#include "./entities.h"
#include "./physics.h"
#include "map_loader.h"
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
                     float vec_thrust, vec3 thrust, uint32_t model_id,
                     char *name) {
  glm_mat4_identity(ENTITY_TRANSFORM[ENTITY_COUNT]);
  glm_mat4_copy(transform, ENTITY_TRANSFORM[ENTITY_COUNT]);
  ENTITY_MODEL[ENTITY_COUNT]      = model_id;
  ENTITY_FACTION[ENTITY_COUNT]    = faction;
  ENTITY_VEC_THRUST[ENTITY_COUNT] = vec_thrust;
  glm_vec3_copy(thrust, ENTITY_THRUST_POWER[ENTITY_COUNT]);
  AI_TARGETS[ENTITY_COUNT]                   = -1;
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
}

void ondeath_split(uint32_t entityId, uint32_t splitter) {
  uint32_t damage = ENTITY_MAXHEALTH[splitter];
  double randdir  = rand() * 10;
  if (ENTITY_MAXHEALTH[entityId] == 1) {
    ondeath_delete(entityId);
    return;
  }
  vec3 dirtosplit;
  glm_vec3_sub(ENTITY_TRANSFORM[splitter][3], ENTITY_TRANSFORM[entityId][3],
               dirtosplit);
  glm_vec3_normalize(dirtosplit);
  ENTITY_SCALE[entityId][0] -= damage;
  ENTITY_SCALE[entityId][1] -= damage;
  ENTITY_SCALE[entityId][2] -= damage;

  // Make the split
  uint32_t firstNew = entity_copy(entityId);

  // Give it some inertia away from the parent body
  vec3 inertia;
  glm_vec3_scale(dirtosplit, damage * 10, inertia);
  glm_vec3_copy(inertia, ENTITY_INERTIA[firstNew]);

  // Position outside of the parent body
  const bvh_aabb *aabb = bvh_root_bounds(&MODEL_HULLS[ENTITY_MODEL[entityId]]);
  vec3 extent = {aabb->max[0] - aabb->min[0], aabb->max[1] - aabb->min[1],
                 aabb->max[2] - aabb->min[2]};
  float size  = fmax(extent[0] * ENTITY_SCALE[entityId][0],
                     fmax(extent[1] * ENTITY_SCALE[entityId][1],
                          extent[2] * ENTITY_SCALE[entityId][2]));
  glm_vec3_scale(dirtosplit, size / 2 + damage * 1.1, dirtosplit);
  ENTITY_TRANSFORM[firstNew][3][0] += dirtosplit[0];
  ENTITY_TRANSFORM[firstNew][3][1] += dirtosplit[1];
  ENTITY_TRANSFORM[firstNew][3][2] += dirtosplit[2];

  // Make it small
  ENTITY_SCALE[firstNew][0]  = damage;
  ENTITY_SCALE[firstNew][1]  = damage;
  ENTITY_SCALE[firstNew][2]  = damage;
  ENTITY_MAXHEALTH[firstNew] = damage;
  ENTITY_HEALTH[firstNew]    = damage;
}

void damage_entity(uint32_t entityId, uint32_t damaging_entityId) {
  uint32_t damage = ENTITY_MAXHEALTH[damaging_entityId];
  ENTITY_HEALTH[entityId] -= damage;
  if (ENTITY_HEALTH[entityId] <= 0) {
    switch (ENTITY_ONDEATH[entityId]) {
    case ONDEATH_EXPLODE: {
      ondeath_explode(entityId);
      break;
    }
    case ONDEATH_SPLIT:
    case ONDEATH_DELETE: {
      ondeath_delete(entityId);
      break;
    }
    }
    return;
  }
  if (ENTITY_ONDEATH[entityId] == ONDEATH_SPLIT) {
    ondeath_split(entityId, damaging_entityId);
  }
}

typedef struct subcollision_userdata {
  uint32_t primhitcount;
  uint32_t prims[32];
  uint32_t a_entityid;
  uint32_t b_entityid;
} subcollision_userdata;
int handle_subcollision(uint32_t prim_index, void *user) {
  subcollision_userdata *subcol = user;
  // One or the other entity may actually have already been deleted by a
  // previous collision
  if (!ENTITY_MODEL[subcol->b_entityid] | !ENTITY_MODEL[subcol->a_entityid]) {
    return 0;
  }
  map_t *map_a           = MODEL_MAP[ENTITY_MODEL[subcol->a_entityid]];
  map_t *map_b           = MODEL_MAP[ENTITY_MODEL[subcol->b_entityid]];
  const map_brush *brush = &map_b->entities[0].brushes[prim_index];
  mat4 inv;
  glm_mat4_inv(ENTITY_TRANSFORM[subcol->b_entityid], inv);
  for (uint32_t t = 0; t < map_a->entity_count; t++) {
    for (uint32_t i = 0; i < map_a->entities[t].brush_count; i++) {
      for (uint32_t k = 0; k < map_a->entities[t].brushes[i].vertex_count;
           k++) {
        // We have a vert within A's frame of reference; need the vert's world
        // location relative to B's frame of reference
        vec3 vert;
        glm_vec3_copy(map_a->entities[t].brushes[i].vertices[k], vert);
        glm_mat4_mulv3(ENTITY_TRANSFORM[subcol->a_entityid], vert, 1, vert);
        glm_mat4_mulv3(inv, vert, 1, vert);
        glm_vec3_div(vert, ENTITY_SCALE[subcol->b_entityid], vert);

        // Check each face of brush B
        uint32_t missed_something = 0;
        for (uint32_t j = 0; j < brush->face_count; j++) {
          map_face *face = &brush->faces[j];
          // Get the heading from the first vertex of the face B being checked
          // to the currently being checked A vertex
          vec3 relvert;
          glm_vec3_sub(vert, brush->vertices[brush->indices[face->first_index]],
                       relvert);
          glm_vec3_normalize(relvert);
          // If the dot product between the currently checked vertex A (relative
          // to the plane B's first vert) and the normal of the plane B is >0
          // then vertex A is in front of face B, meaning we must not be inside
          // the brush B
          if (glm_vec3_dot(face->normal, relvert) > 0) {
            missed_something = 1;
            break;
          }
        }
        // If we were behind all the planes, we are inside the brush.
        if (!missed_something) {
          uint32_t entity_a = subcol->a_entityid;
          uint32_t entity_b = subcol->b_entityid;

          if (ENTITY_HEALTH[entity_b] <= 0 || ENTITY_HEALTH[entity_a] <= 0) {
            return 0;
          }

          damage_entity(entity_b, entity_a);
          damage_entity(entity_a, entity_b);
        }
      }
    }
  }
  return 0;
}

int handle_collision(uint32_t prim_index_a, uint32_t prim_index_b, void *user) {
  uint32_t entity_a = AABB_IDS[prim_index_a];
  uint32_t entity_b = AABB_IDS[prim_index_b];
  if (entity_b != entity_a) {

    const bvh_t *bvh_a = &MODEL_HULLS[ENTITY_MODEL[entity_a]];
    const bvh_t *bvh_b = &MODEL_HULLS[ENTITY_MODEL[entity_b]];
    // If A is bigger than B, swap them. We want a small hull to test against a
    // large BVH
    const bvh_aabb *hull_a = bvh_root_bounds(bvh_a);
    if (hull_a == 0) {
      printf("Collision with entity already dead!\n");
      return 0;
    }
    uint32_t area_a =
        (hull_a->max[0] - hull_a->min[0] * ENTITY_SCALE[entity_a][0]) *
        (hull_a->max[1] - hull_a->min[1] * ENTITY_SCALE[entity_a][1]) *
        (hull_a->max[2] - hull_a->min[2] * ENTITY_SCALE[entity_a][2]);
    const bvh_aabb *hull_b = bvh_root_bounds(bvh_b);
    if (hull_b == 0) {
      printf("Collision with entity already dead!\n");
      return 0;
    }
    uint32_t area_b =
        (hull_b->max[0] - hull_b->min[0] * ENTITY_SCALE[entity_b][0]) *
        (hull_b->max[1] - hull_b->min[1] * ENTITY_SCALE[entity_b][1]) *
        (hull_b->max[2] - hull_b->min[2] * ENTITY_SCALE[entity_b][2]);
    if (area_a > area_b) {
      uint32_t tmp = entity_b;
      entity_b     = entity_a;
      entity_a     = tmp;

      bvh_b  = bvh_a;
      hull_a = hull_b;
    }
    // Calculate an offset that applies A's transform and the inverse of B's
    // tarnsform.
    mat4 *transform_b = &ENTITY_TRANSFORM[entity_b];
    vec3 offset;
    glm_vec3_copy(ENTITY_TRANSFORM[entity_a][3], offset);
    mat4 inv;
    glm_mat4_inv(*transform_b, inv);
    glm_mat4_mulv3(inv, offset, 1.0, offset);

    // Create a bounding box derived from A transposed into B's frame of
    // reference. Divide by B's scale to account for B's BVH being in the "unit"
    // form.
    bvh_aabb hull_a_check = {
        .min = {((hull_a->min[0] + offset[0]) / ENTITY_SCALE[entity_b][0]),
                ((hull_a->min[1] + offset[1]) / ENTITY_SCALE[entity_b][1]),
                ((hull_a->min[2] + offset[2]) / ENTITY_SCALE[entity_b][2])},
        .max = {((hull_a->max[0] + offset[0]) / ENTITY_SCALE[entity_b][0]),
                ((hull_a->max[1] + offset[1]) / ENTITY_SCALE[entity_b][1]),
                ((hull_a->max[2] + offset[2]) / ENTITY_SCALE[entity_b][2])}};
    // Test our small hull against our large BVH
    subcollision_userdata subcol = {.a_entityid = entity_a,
                                    .b_entityid = entity_b};
    bvh_aabb_query(bvh_b, &hull_a_check, handle_subcollision, &subcol);
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
            make_entity(ENTITY_TRANSFORM[ship], -1, (vec3){1., 0., 0.}, 0,
                        (vec3){-1., -1., -1.}, GUN_MODEL[ship], "Bullet");
        vec3 inertia = {0, 0, GUN_SPEED[ship]};
        glm_mat4_mulv3(ENTITY_TRANSFORM[ship], inertia, 0., inertia);
        glm_vec3_copy(inertia, ENTITY_INERTIA[bullet]);
        glm_translate(ENTITY_TRANSFORM[bullet],
                      (vec3){0., 0., GUN_SPEED[ship] * 0.01});
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
