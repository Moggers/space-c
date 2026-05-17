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
map_t *MODEL_MAP[MAX_MODELS];

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
    if ((ENTITY_DEAD[ship] > 0) | (ENTITY_THRUST_POWER[ship][0] == -1) |
        (ENTITY_DOCKING[ship][0])) {
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
    if (ENTITY_DOCKING[ship][0]) {
      continue;
    }
    ENTITY_TRANSFORM[ship][3][0] += ENTITY_INERTIA[ship][0] * delta_time;
    ENTITY_TRANSFORM[ship][3][1] += ENTITY_INERTIA[ship][1] * delta_time;
    ENTITY_TRANSFORM[ship][3][2] += ENTITY_INERTIA[ship][2] * delta_time;
  }
}

typedef struct ri_userdata {
  vec3 incidence;
  map_face *incident_face;
  map_brush *incident_brush;
  uint32_t incident_entity;
  float best_t;
} ri_userdata;
typedef struct risu_userdata {
  map_t *map;
  ri_userdata *out;
  uint32_t entity;
} risu_userdata;
int ray_intersection_subcb(uint32_t prim, bvh_ray *r, void *userdata) {
  risu_userdata *risu = (risu_userdata *)userdata;
  map_brush *b        = &risu->map->entities[0].brushes[prim];

  for (uint32_t i = 0; i < b->face_count; i++) {
    map_face *f = &b->faces[i];
    float *vert = b->vertices[b->indices[b->faces[i].first_index]];

    vec3 relvert;
    glm_vec3_sub(vert, r->origin, relvert);

    float dot1 = glm_vec3_dot(relvert, f->normal);
    float dot2 = glm_vec3_dot(f->normal, r->dir);

    // Either backface or more-or-less perpendicular with plane
    if (dot2 > -0.001) {
      continue;
    }
    float t = dot1 / dot2;

    // The incidence is behind us.
    if (t < 0) {
      continue;
    }

    vec3 offset;
    glm_vec3_scale(r->dir, t, offset);
    vec3 incidence;
    glm_vec3_add(r->origin, offset, incidence);

    // Check if within bounds of polygon
    uint32_t missed = 0;
    // For each vertex
    for (uint32_t k = 0; k < f->index_count; k++) {
      vec3 relvertthing, outward, relinc;
      // Get the heading from vertex to incidence
      glm_vec3_sub(incidence, b->vertices[b->indices[f->first_index + k]],
                   relinc);
      // Get the heading from the vertex to the next vetex (the line we want the
      // cross product of)
      glm_vec3_sub(
          b->vertices[b->indices[f->first_index + ((k + 1) % f->index_count)]],
          b->vertices[b->indices[f->first_index + k]], relvertthing);
      // Get the cross product between line dir and normal to find normal facing
      // out of the polygon
      glm_vec3_cross(relvertthing, f->normal, outward);

      // If the dot product between incidence relative to the vertex and the
      // direction out from the line the vertex starts is above 0 then we are
      // outside the polygon
      if (glm_vec3_dot(outward, relinc) > 0) {
        missed = 1;
      }
    }
    if (missed == 1) {
      continue;
    }

    // Is this the first intersection or closer than any found so far
    if (t < risu->out->best_t) {
      risu->out->best_t          = t;
      risu->out->incident_face   = f;
      risu->out->incident_entity = risu->entity;
      risu->out->incident_brush  = b;
      glm_vec3_copy(incidence, risu->out->incidence);
    }
  }
  return 0;
}
int ray_intersection_cb(uint32_t prim, bvh_ray *r, void *userdata) {
  uint32_t striking = AABB_IDS[prim];
  bvh_ray ray;
  mat4 inv;
  glm_mat4_inv(ENTITY_TRANSFORM[striking], inv);
  vec3 no, nd;
  glm_mat4_mulv3(inv, r->origin, 1, no);
  glm_vec3_div(no, ENTITY_SCALE[striking], no);
  glm_vec3_rotate_m4(inv, r->dir, nd);
  bvh_ray_init(&ray, no, nd, 0, 100000);
  ri_userdata new_out;
  memcpy(&new_out, userdata, sizeof(ri_userdata));
  risu_userdata risu = {
      .map    = MODEL_MAP[ENTITY_MODEL[striking]],
      .entity = striking,
      .out    = &new_out,
  };
  bvh_ray_query_tight(&MODEL_HULLS[ENTITY_MODEL[striking]], &ray,
                      ray_intersection_subcb, &risu);
  if (new_out.incident_entity != 0 &&
      glm_vec3_distance(new_out.incidence, r->origin) <
          glm_vec3_distance(((ri_userdata *)userdata)->incidence, r->origin)) {
    memcpy(userdata, &new_out, sizeof(ri_userdata));
  }
  return 0;
}

typedef struct IntersectionResult {
  uint32_t incident_entity;
  map_brush *incident_brush;
  map_face *incident_face;
  vec3 incident_point;
} IntersectionResult;

IntersectionResult ray_intersection(vec3 origin, vec3 dir) {
  bvh_ray ray;
  bvh_ray_init(&ray, origin, dir, 0, 100);
  ri_userdata out = {.best_t = FLT_MAX};
  bvh_ray_query_tight(&ENTITY_BVH, &ray, ray_intersection_cb, &out);
  if (out.best_t != FLT_MAX) {
    glm_vec3_mulv(ENTITY_SCALE[out.incident_entity], out.incidence,
                  out.incidence);
    glm_mat4_mulv3(ENTITY_TRANSFORM[out.incident_entity], out.incidence, 1,
                   out.incidence);
  }
  IntersectionResult rval = {
      .incident_brush  = out.incident_brush,
      .incident_face   = out.incident_face,
      .incident_entity = out.incident_entity,
  };
  glm_vec3_copy(out.incidence, rval.incident_point);
  return rval;
}

void find_correction_location(vec3 start, vec3 dest, float gapsize, vec3 out) {
  vec3 dir;
  float disttodesired = glm_vec3_distance(dest, start);
  glm_vec3_sub(dest, start, dir);
  glm_vec3_normalize(dir);
  IntersectionResult r = ray_intersection(start, dir);
  glm_vec3_copy(r.incident_point, ENTITY_TRANSFORM[DEBUG_MARKERS[2]][3]);
  if (r.incident_entity == 0 ||
      glm_vec3_distance(r.incident_point, start) > disttodesired) {
    glm_vec3_copy(dest, out);
    return;
  }
  if (r.incident_entity > 0) {
    vec3 transformednorm;
    glm_vec3_rotate_m4(ENTITY_TRANSFORM[r.incident_entity],
                       r.incident_face->normal, transformednorm);
    vec3 heading_dir;
    // TODO: Should really just subtract norm * (headingdir . norm) from
    // headingdir instead of using a vector triple product
    glm_vec3_cross(dir, transformednorm, heading_dir);
    glm_vec3_inv(heading_dir);
    glm_vec3_cross(heading_dir, transformednorm, heading_dir);
    glm_vec3_normalize(heading_dir);

    // SLOP STARTS HERE
    // If the point projected onto the hit plane lies within the polygon, we
    // cannot navigate in its direction along the polygon, since we'll end up
    // stuck at a point; in that case we instead flip the heading to navigate
    // along the face *away* from the target.
    vec3 target_proj;
    {
      vec3 dest_rel;
      glm_vec3_sub(dest, r.incident_point, dest_rel);
      float along_n = glm_vec3_dot(dest_rel, transformednorm);
      vec3 normal_component;
      glm_vec3_scale(transformednorm, along_n, normal_component);
      glm_vec3_sub(dest, normal_component, target_proj);
    }
    int in_shadow = 1;
    for (uint32_t k = 0; k < r.incident_face->index_count; k++) {
      uint32_t i0 = r.incident_brush->indices[r.incident_face->first_index + k];
      uint32_t i1 =
          r.incident_brush->indices[r.incident_face->first_index +
                                    ((k + 1) % r.incident_face->index_count)];
      vec3 v0, v1;
      glm_mat4_mulv3(ENTITY_TRANSFORM[r.incident_entity],
                     r.incident_brush->vertices[i0], 1, v0);
      glm_mat4_mulv3(ENTITY_TRANSFORM[r.incident_entity],
                     r.incident_brush->vertices[i1], 1, v1);
      vec3 edge, outward, rel;
      glm_vec3_sub(v1, v0, edge);
      glm_vec3_cross(edge, transformednorm, outward);
      glm_vec3_sub(target_proj, v0, rel);
      if (glm_vec3_dot(rel, outward) > 0) {
        in_shadow = 0;
        break;
      }
    }
    if (in_shadow) {
      glm_vec3_inv(heading_dir);
    }
    // SLOP ENDS HERE

    float best_t = FLT_MAX;
    for (uint32_t j = 0; j < r.incident_brush->face_count; j++) {
      map_face *f = &r.incident_brush->faces[j];

      // Ignore interior faces; we can't navigate out via those.
      if(f->is_interior) {
        continue;
      }

      // Get the normal in world orientation
      vec3 normalworld;
      glm_vec3_rotate_m4(ENTITY_TRANSFORM[r.incident_entity], f->normal,
                         normalworld);

      float *vert =
          r.incident_brush
              ->vertices[r.incident_brush
                             ->indices[r.incident_brush->faces[j].first_index]];
      // Get the vertex in worldspace
      vec3 relvert;
      glm_mat4_mulv3(ENTITY_TRANSFORM[r.incident_entity], vert, 1, relvert);
      // And then get it relative to the origin..
      glm_vec3_sub(relvert, r.incident_point, relvert);
      // Handy dandy "distance to project heading to intersect with plane"
      // equation
      float dot1 = glm_vec3_dot(relvert, normalworld);
      float dot2 = glm_vec3_dot(heading_dir, normalworld);
      float t    = dot1 / dot2;
      // t will be + if we could trace this path to exit the front side
      // of a plane. Add a bit extra to avoid float rubbish.
      if (t < 0.001) {
        continue;
      }
      // Try to see if this is the first plane "so far" which we have the
      // opportunity to exit
      if (t < best_t) {
        best_t = t;
      }
    }
    // Pretty sure we should have found at least one so maybe don't need
    // to check if best_t is valid..
    glm_vec3_scale(transformednorm, gapsize, transformednorm);
    glm_vec3_scale(heading_dir, best_t + gapsize, heading_dir);
    glm_vec3_add(heading_dir, r.incident_point, heading_dir);
    glm_vec3_add(heading_dir, transformednorm, heading_dir);
    glm_vec3_copy(heading_dir, out);
  }
}
#endif
