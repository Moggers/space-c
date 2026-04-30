/* bvh.h - single-header pure C BVH library (3D)
 *
 * Build a bounding-volume hierarchy over axis-aligned bounding boxes and
 * query it with rays, AABBs, points, or against another BVH. Intended for
 * collision broadphase, ray casting, frustum / region selection, etc.
 *
 * Usage
 * -----
 *   In ONE translation unit:
 *       #define BVH_IMPLEMENTATION
 *       #include "bvh.h"
 *
 *   In all other translation units:
 *       #include "bvh.h"
 *
 * Build options can be customized before including the implementation:
 *       #define BVH_ASSERT(x)   my_assert(x)
 *       #define BVH_MALLOC(sz)  my_malloc(sz)
 *       #define BVH_FREE(p)     my_free(p)
 *
 * The library has no external dependencies beyond the C standard library
 * and is C99 compatible.
 *
 * Example
 * -------
 *
 *     #define BVH_IMPLEMENTATION
 *     #include "bvh.h"
 *
 *     static int on_hit(uint32_t prim, void* user) {
 *         printf("query overlaps primitive %u\n", prim);
 *         (*(int*)user)++;
 *         return 0;            // return non-zero to halt early
 *     }
 *
 *     int main(void) {
 *         bvh_aabb prims[3] = {
 *             {{0,0,0},{1,1,1}},
 *             {{2,2,2},{3,3,3}},
 *             {{0.5f,0.5f,0.5f},{1.5f,1.5f,1.5f}},
 *         };
 *
 *         bvh_t bvh = {0};
 *         bvh_build(&bvh, prims, 3, NULL);
 *
 *         // AABB-vs-BVH collision: which primitives overlap this box?
 *         bvh_aabb query = {{0,0,0},{1,1,1}};
 *         int hits = 0;
 *         bvh_aabb_query(&bvh, &query, on_hit, &hits);
 *
 *         // Broadphase: every overlapping pair within the BVH.
 *         // bvh_self_overlap(&bvh, on_pair, &user_state);
 *
 *         // Per-frame motion: refit is O(N) and ~30x cheaper than rebuild.
 *         // bvh_refit(&bvh, updated_prims);
 *
 *         bvh_free(&bvh);
 *     }
 *
 * Public domain / 0BSD - see end of file.
 */

#ifndef BVH_H_INCLUDED
#define BVH_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Types
 * ============================================================ */

typedef struct {
    float min[3];
    float max[3];
} bvh_aabb;

/* Node layout: 32 bytes.
 *   prim_count == 0  -> internal node; left_first is left-child index.
 *                       The right child always lives at left_first + 1.
 *   prim_count  > 0  -> leaf; left_first is the start index into
 *                       bvh_t::prim_idx, prim_count is the number of
 *                       primitives in the leaf. */
typedef struct {
    bvh_aabb bounds;
    uint32_t left_first;
    uint32_t prim_count;
} bvh_node;

typedef struct {
    bvh_node* nodes;
    uint32_t  node_count;
    uint32_t  node_cap;
    uint32_t* prim_idx;      /* permutation of [0 .. prim_count) */
    bvh_aabb* prim_aabbs;    /* copy of per-primitive AABBs, indexed by ORIGINAL prim index */
    uint32_t  prim_count;
} bvh_t;

typedef struct {
    int      sah_bins;        /* SAH bin count per axis (default 16, clamped 2..64) */
    uint32_t leaf_threshold;  /* primitives per leaf at which to stop splitting (default 4) */
} bvh_build_opts;

/* ============================================================
 *  Construction / destruction
 * ============================================================ */

/* Build a BVH over `prim_count` AABBs. The library copies the AABB array
 * so the caller can free or modify the input afterwards; queries report
 * primitives whose stored AABB still satisfies the query.
 *
 * `bvh` is overwritten and any prior contents are freed, so it is safe to
 * call repeatedly to rebuild. `opts` may be NULL for defaults.
 *
 * Returns 0 on success, non-zero on allocation failure. */
int  bvh_build(bvh_t* bvh,
               const bvh_aabb* prim_aabbs,
               uint32_t prim_count,
               const bvh_build_opts* opts);

void bvh_free(bvh_t* bvh);

/* Re-fit an existing BVH after primitive AABBs have moved, keeping the
 * tree topology unchanged. O(N), no allocations.
 *
 * `new_prim_aabbs` must have the same length as the array originally
 * passed to bvh_build; primitives keep their original indices.
 *
 * Refitting is much cheaper than rebuilding but does not re-balance the
 * tree. Query performance degrades as motion accumulates, so call
 * bvh_build occasionally (e.g. every 30-100 frames, or when bounds-area
 * grows past some threshold) to restore quality. */
void bvh_refit(bvh_t* bvh, const bvh_aabb* new_prim_aabbs);

/* Bounds of the root node, or NULL for an empty BVH. */
const bvh_aabb* bvh_root_bounds(const bvh_t* bvh);

/* ============================================================
 *  Ray queries
 * ============================================================ */

typedef struct {
    float origin[3];
    float dir[3];
    float inv_dir[3];   /* 1.0f / dir; populated by bvh_ray_init */
    float tmin, tmax;
} bvh_ray;

void bvh_ray_init(bvh_ray* r,
                  const float origin[3], const float dir[3],
                  float tmin, float tmax);

/* Per-leaf-primitive callback during a ray query.
 *   - `prim_index` is the original primitive index (as supplied to bvh_build).
 *   - The callback should test the ray against the primitive and, on a closer
 *     hit, shrink `ray->tmax` so that the traversal can prune farther boxes.
 *   - Return non-zero to halt the traversal early (e.g. for any-hit /
 *     shadow rays); return zero to continue searching for a closer hit. */
typedef int (*bvh_ray_cb)(uint32_t prim_index, bvh_ray* ray, void* user);

void bvh_ray_query(const bvh_t* bvh, bvh_ray* ray, bvh_ray_cb cb, void* user);

/* ============================================================
 *  AABB collision queries
 * ============================================================ */

/* Per-primitive callback for AABB / point queries. Return non-zero to halt. */
typedef int (*bvh_aabb_cb)(uint32_t prim_index, void* user);

/* Report every primitive whose AABB overlaps `box`. */
void bvh_aabb_query(const bvh_t* bvh, const bvh_aabb* box,
                    bvh_aabb_cb cb, void* user);

/* Report every primitive whose AABB contains `point`. */
void bvh_point_query(const bvh_t* bvh, const float point[3],
                     bvh_aabb_cb cb, void* user);

/* Per-pair callback. Return non-zero to halt. */
typedef int (*bvh_pair_cb)(uint32_t a_prim, uint32_t b_prim, void* user);

/* Pairwise overlap between two BVHs. For every pair (a, b) such that
 * primitive `a` in `bvh_a` and primitive `b` in `bvh_b` have overlapping
 * AABBs, the callback fires once with (a, b). */
void bvh_overlap_query(const bvh_t* bvh_a, const bvh_t* bvh_b,
                       bvh_pair_cb cb, void* user);

/* Self-overlap: every distinct unordered pair (i, j) of primitives in the
 * same BVH whose AABBs overlap is reported exactly once. Useful as a
 * broadphase for N-body collision detection. */
void bvh_self_overlap(const bvh_t* bvh, bvh_pair_cb cb, void* user);

/* ============================================================
 *  Helpers
 * ============================================================ */

/* AABB / triangle helpers - convenient when feeding triangle meshes. */
void bvh_triangle_aabb(const float v0[3], const float v1[3], const float v2[3],
                       bvh_aabb* out);

/* Möller–Trumbore ray-triangle test. Returns 1 on a hit strictly within
 * (ray->tmin, ray->tmax); writes hit distance and barycentrics if non-NULL. */
int bvh_ray_triangle(const bvh_ray* ray,
                     const float v0[3], const float v1[3], const float v2[3],
                     float* out_t, float* out_u, float* out_v);

/* Pure AABB-vs-AABB overlap helper (closed intervals). */
static inline int bvh_aabb_overlap(const bvh_aabb* a, const bvh_aabb* b) {
    return (a->min[0] <= b->max[0]) & (a->max[0] >= b->min[0]) &
           (a->min[1] <= b->max[1]) & (a->max[1] >= b->min[1]) &
           (a->min[2] <= b->max[2]) & (a->max[2] >= b->min[2]);
}

#ifdef __cplusplus
}
#endif

/* ============================================================
 *  Implementation
 * ============================================================ */
#ifdef BVH_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef BVH_ASSERT
#  include <assert.h>
#  define BVH_ASSERT(x) assert(x)
#endif
#ifndef BVH_MALLOC
#  define BVH_MALLOC(sz) malloc(sz)
#endif
#ifndef BVH_FREE
#  define BVH_FREE(p)    free(p)
#endif

#define BVH__MAX_BINS       64
#define BVH__TRAVERSE_STACK 128   /* >= 2 * max BVH depth (~64) */

/* ---- AABB primitives ---- */

static void bvh__aabb_reset(bvh_aabb* b) {
    b->min[0] = b->min[1] = b->min[2] =  FLT_MAX;
    b->max[0] = b->max[1] = b->max[2] = -FLT_MAX;
}

static void bvh__aabb_grow(bvh_aabb* b, const bvh_aabb* o) {
    if (o->min[0] < b->min[0]) b->min[0] = o->min[0];
    if (o->min[1] < b->min[1]) b->min[1] = o->min[1];
    if (o->min[2] < b->min[2]) b->min[2] = o->min[2];
    if (o->max[0] > b->max[0]) b->max[0] = o->max[0];
    if (o->max[1] > b->max[1]) b->max[1] = o->max[1];
    if (o->max[2] > b->max[2]) b->max[2] = o->max[2];
}

static float bvh__aabb_half_area(const bvh_aabb* b) {
    float ex = b->max[0] - b->min[0];
    float ey = b->max[1] - b->min[1];
    float ez = b->max[2] - b->min[2];
    /* SAH only needs a quantity proportional to surface area; the
     * factor of 2 is constant across all comparisons. */
    return ex * ey + ey * ez + ez * ex;
}

/* ---- Build context ---- */

typedef struct {
    bvh_t*           bvh;
    const bvh_aabb*  src_aabbs;
    float          (*centroids)[3];
    int              sah_bins;
    uint32_t         leaf_threshold;
} bvh__build;

typedef struct {
    bvh_aabb bounds;
    uint32_t count;
} bvh__bin;

static void bvh__node_compute_bounds(bvh__build* B, bvh_node* node) {
    bvh__aabb_reset(&node->bounds);
    uint32_t s = node->left_first;
    uint32_t e = s + node->prim_count;
    for (uint32_t i = s; i < e; ++i) {
        bvh__aabb_grow(&node->bounds, &B->src_aabbs[B->bvh->prim_idx[i]]);
    }
}

/* Pick best (axis, split-position) using SAH binning over centroid
 * coordinates. Returns the resulting split cost, or FLT_MAX if no
 * usable split exists (e.g. all centroids coincident). */
static float bvh__find_split(bvh__build* B, const bvh_node* node,
                             int* out_axis, float* out_split) {
    uint32_t start = node->left_first;
    uint32_t end   = start + node->prim_count;

    /* Centroid bounds for this node. */
    float cmin[3] = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    float cmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (uint32_t i = start; i < end; ++i) {
        const float* c = B->centroids[B->bvh->prim_idx[i]];
        for (int k = 0; k < 3; ++k) {
            if (c[k] < cmin[k]) cmin[k] = c[k];
            if (c[k] > cmax[k]) cmax[k] = c[k];
        }
    }

    int   best_axis  = -1;
    float best_split = 0.0f;
    float best_cost  = FLT_MAX;
    int   bins       = B->sah_bins;

    bvh__bin bin[BVH__MAX_BINS];
    bvh_aabb right_bounds[BVH__MAX_BINS];
    uint32_t right_count [BVH__MAX_BINS];

    for (int axis = 0; axis < 3; ++axis) {
        float extent = cmax[axis] - cmin[axis];
        if (extent <= 0.0f) continue;

        /* Bin primitives by centroid on this axis. */
        for (int i = 0; i < bins; ++i) {
            bvh__aabb_reset(&bin[i].bounds);
            bin[i].count = 0;
        }
        float bin_scale = (float)bins / extent;
        for (uint32_t i = start; i < end; ++i) {
            uint32_t pi = B->bvh->prim_idx[i];
            int b = (int)((B->centroids[pi][axis] - cmin[axis]) * bin_scale);
            if (b < 0) b = 0;
            if (b >= bins) b = bins - 1;
            bvh__aabb_grow(&bin[b].bounds, &B->src_aabbs[pi]);
            bin[b].count++;
        }

        /* Right-to-left sweep: accumulate bounds/counts of bins [i..bins-1]. */
        bvh_aabb racc; bvh__aabb_reset(&racc);
        uint32_t rcnt = 0;
        for (int i = bins - 1; i >= 0; --i) {
            if (bin[i].count) { bvh__aabb_grow(&racc, &bin[i].bounds); rcnt += bin[i].count; }
            right_bounds[i] = racc;
            right_count [i] = rcnt;
        }

        /* Left-to-right sweep with on-the-fly evaluation of bins-1 candidate splits. */
        bvh_aabb lacc; bvh__aabb_reset(&lacc);
        uint32_t lcnt = 0;
        for (int i = 0; i < bins - 1; ++i) {
            if (bin[i].count) { bvh__aabb_grow(&lacc, &bin[i].bounds); lcnt += bin[i].count; }
            uint32_t rc = right_count[i + 1];
            if (lcnt == 0 || rc == 0) continue;
            float cost = lcnt * bvh__aabb_half_area(&lacc) +
                         rc   * bvh__aabb_half_area(&right_bounds[i + 1]);
            if (cost < best_cost) {
                best_cost  = cost;
                best_axis  = axis;
                best_split = cmin[axis] + extent * (float)(i + 1) / (float)bins;
            }
        }
    }

    *out_axis  = best_axis;
    *out_split = best_split;
    return best_cost;
}

/* In-place Hoare partition of prim_idx[start..end) by centroid[axis] < split.
 * Returns the index of the first element of the right side. */
static uint32_t bvh__partition(bvh__build* B, uint32_t start, uint32_t end,
                               int axis, float split) {
    uint32_t i = start;
    uint32_t j = end;
    uint32_t* idx = B->bvh->prim_idx;
    while (i < j) {
        if (B->centroids[idx[i]][axis] < split) {
            ++i;
        } else {
            --j;
            uint32_t t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
    }
    return i;
}

static void bvh__subdivide(bvh__build* B, uint32_t node_idx) {
    bvh_node* node = &B->bvh->nodes[node_idx];
    if (node->prim_count <= B->leaf_threshold) return;

    int   axis;
    float split;
    float split_cost = bvh__find_split(B, node, &axis, &split);

    /* Compare to the cost of keeping this as a leaf. SAH leaf cost is
     * N * area; we already dropped the constant K_I in split cost. */
    float leaf_cost = (float)node->prim_count * bvh__aabb_half_area(&node->bounds);
    if (axis < 0 || split_cost >= leaf_cost) return;

    uint32_t start = node->left_first;
    uint32_t end   = start + node->prim_count;
    uint32_t mid   = bvh__partition(B, start, end, axis, split);

    /* Pathological case: the chosen split left one side empty. Bail to
     * a leaf rather than infinitely recurse. */
    if (mid == start || mid == end) return;

    uint32_t left  = B->bvh->node_count++;
    uint32_t right = B->bvh->node_count++;
    BVH_ASSERT(B->bvh->node_count <= B->bvh->node_cap);

    B->bvh->nodes[left ].left_first = start;
    B->bvh->nodes[left ].prim_count = mid - start;
    B->bvh->nodes[right].left_first = mid;
    B->bvh->nodes[right].prim_count = end - mid;

    /* Reuse `node` index but the pointer may have been invalidated by
     * the increment of node_count (we did not realloc, so it has not).
     * Convert this node to internal: prim_count = 0, left_first = left. */
    node = &B->bvh->nodes[node_idx];
    node->left_first = left;
    node->prim_count = 0;

    bvh__node_compute_bounds(B, &B->bvh->nodes[left]);
    bvh__node_compute_bounds(B, &B->bvh->nodes[right]);

    bvh__subdivide(B, left);
    bvh__subdivide(B, right);
}

/* ---- Public construction ---- */

void bvh_free(bvh_t* bvh) {
    if (!bvh) return;
    if (bvh->nodes)      BVH_FREE(bvh->nodes);
    if (bvh->prim_idx)   BVH_FREE(bvh->prim_idx);
    if (bvh->prim_aabbs) BVH_FREE(bvh->prim_aabbs);
    bvh->nodes = NULL;
    bvh->prim_idx = NULL;
    bvh->prim_aabbs = NULL;
    bvh->node_count = bvh->node_cap = bvh->prim_count = 0;
}

int bvh_build(bvh_t* bvh, const bvh_aabb* prim_aabbs, uint32_t prim_count,
              const bvh_build_opts* opts) {
    BVH_ASSERT(bvh);

    if (prim_count == 0) { bvh_free(bvh); return 0; }
    BVH_ASSERT(prim_aabbs);

    bvh_build_opts o;
    o.sah_bins       = (opts && opts->sah_bins       > 0) ? opts->sah_bins       : 16;
    o.leaf_threshold = (opts && opts->leaf_threshold > 0) ? opts->leaf_threshold : 4;
    if (o.sah_bins < 2)             o.sah_bins = 2;
    if (o.sah_bins > BVH__MAX_BINS) o.sah_bins = BVH__MAX_BINS;

    /* Worst-case node count for a binary BVH with N leaves of size 1
     * is 2N - 1; we round up to 2N for a clean allocation. */
    uint32_t need_node_cap = 2u * prim_count;

    /* Reuse persistent buffers if they are already sized correctly.
     * This makes back-to-back rebuilds with the same primitive count
     * allocation-free. The centroid scratch buffer is per-build only. */
    int need_realloc = (bvh->prim_count != prim_count) ||
                       (bvh->node_cap   <  need_node_cap) ||
                       !bvh->nodes || !bvh->prim_idx || !bvh->prim_aabbs;

    if (need_realloc) {
        bvh_free(bvh);
        bvh->nodes      = (bvh_node*)BVH_MALLOC(sizeof(bvh_node) * need_node_cap);
        bvh->prim_idx   = (uint32_t*)BVH_MALLOC(sizeof(uint32_t) * prim_count);
        bvh->prim_aabbs = (bvh_aabb*)BVH_MALLOC(sizeof(bvh_aabb) * prim_count);
        if (!bvh->nodes || !bvh->prim_idx || !bvh->prim_aabbs) {
            bvh_free(bvh);
            return 1;
        }
        bvh->node_cap   = need_node_cap;
        bvh->prim_count = prim_count;
    }

    float (*centroids)[3] = (float(*)[3])BVH_MALLOC(sizeof(float) * 3 * prim_count);
    if (!centroids) { bvh_free(bvh); return 1; }

    memcpy(bvh->prim_aabbs, prim_aabbs, sizeof(bvh_aabb) * prim_count);

    for (uint32_t i = 0; i < prim_count; ++i) {
        bvh->prim_idx[i] = i;
        const bvh_aabb* a = &prim_aabbs[i];
        centroids[i][0] = 0.5f * (a->min[0] + a->max[0]);
        centroids[i][1] = 0.5f * (a->min[1] + a->max[1]);
        centroids[i][2] = 0.5f * (a->min[2] + a->max[2]);
    }

    /* Initialize root. */
    bvh->node_count = 1;
    bvh->nodes[0].left_first = 0;
    bvh->nodes[0].prim_count = prim_count;

    bvh__build B;
    B.bvh            = bvh;
    B.src_aabbs      = prim_aabbs;
    B.centroids      = centroids;
    B.sah_bins       = o.sah_bins;
    B.leaf_threshold = o.leaf_threshold;

    bvh__node_compute_bounds(&B, &bvh->nodes[0]);
    bvh__subdivide(&B, 0);

    BVH_FREE(centroids);
    return 0;
}

const bvh_aabb* bvh_root_bounds(const bvh_t* bvh) {
    if (!bvh || bvh->node_count == 0) return NULL;
    return &bvh->nodes[0].bounds;
}

void bvh_refit(bvh_t* bvh, const bvh_aabb* new_prim_aabbs) {
    if (!bvh || bvh->node_count == 0) return;
    BVH_ASSERT(new_prim_aabbs);

    memcpy(bvh->prim_aabbs, new_prim_aabbs, sizeof(bvh_aabb) * bvh->prim_count);

    /* Walking node indices in reverse order processes children before
     * parents: bvh__subdivide always allocates child nodes after the
     * parent, so descendants always have higher indices. */
    for (uint32_t i = bvh->node_count; i-- > 0; ) {
        bvh_node* n = &bvh->nodes[i];
        if (n->prim_count > 0) {
            bvh__aabb_reset(&n->bounds);
            uint32_t s = n->left_first;
            uint32_t e = s + n->prim_count;
            for (uint32_t k = s; k < e; ++k) {
                bvh__aabb_grow(&n->bounds, &bvh->prim_aabbs[bvh->prim_idx[k]]);
            }
        } else {
            uint32_t l = n->left_first;
            n->bounds = bvh->nodes[l].bounds;
            bvh__aabb_grow(&n->bounds, &bvh->nodes[l + 1].bounds);
        }
    }
}

/* ---- Ray queries ---- */

void bvh_ray_init(bvh_ray* r, const float origin[3], const float dir[3],
                  float tmin, float tmax) {
    r->origin[0] = origin[0]; r->origin[1] = origin[1]; r->origin[2] = origin[2];
    r->dir   [0] = dir   [0]; r->dir   [1] = dir   [1]; r->dir   [2] = dir   [2];
    /* Division by zero on an axis yields +/-inf, which the slab test
     * handles correctly via min/max comparisons. */
    r->inv_dir[0] = 1.0f / dir[0];
    r->inv_dir[1] = 1.0f / dir[1];
    r->inv_dir[2] = 1.0f / dir[2];
    r->tmin = tmin;
    r->tmax = tmax;
}

static inline float bvh__minf(float a, float b) { return a < b ? a : b; }
static inline float bvh__maxf(float a, float b) { return a > b ? a : b; }

/* Returns near-t of intersection or FLT_MAX if no hit. */
static float bvh__ray_aabb(const bvh_ray* r, const bvh_aabb* b) {
    float tx1 = (b->min[0] - r->origin[0]) * r->inv_dir[0];
    float tx2 = (b->max[0] - r->origin[0]) * r->inv_dir[0];
    float tmin = bvh__minf(tx1, tx2);
    float tmax = bvh__maxf(tx1, tx2);

    float ty1 = (b->min[1] - r->origin[1]) * r->inv_dir[1];
    float ty2 = (b->max[1] - r->origin[1]) * r->inv_dir[1];
    tmin = bvh__maxf(tmin, bvh__minf(ty1, ty2));
    tmax = bvh__minf(tmax, bvh__maxf(ty1, ty2));

    float tz1 = (b->min[2] - r->origin[2]) * r->inv_dir[2];
    float tz2 = (b->max[2] - r->origin[2]) * r->inv_dir[2];
    tmin = bvh__maxf(tmin, bvh__minf(tz1, tz2));
    tmax = bvh__minf(tmax, bvh__maxf(tz1, tz2));

    if (tmax >= bvh__maxf(tmin, r->tmin) && tmin < r->tmax) return tmin;
    return FLT_MAX;
}

void bvh_ray_query(const bvh_t* bvh, bvh_ray* ray, bvh_ray_cb cb, void* user) {
    if (!bvh || bvh->node_count == 0) return;

    uint32_t stack[BVH__TRAVERSE_STACK];
    int sp = 0;
    uint32_t node_idx = 0;

    for (;;) {
        const bvh_node* node = &bvh->nodes[node_idx];
        if (node->prim_count > 0) {
            uint32_t s = node->left_first;
            for (uint32_t i = 0; i < node->prim_count; ++i) {
                if (cb(bvh->prim_idx[s + i], ray, user)) return;
            }
            if (sp == 0) return;
            node_idx = stack[--sp];
            continue;
        }

        uint32_t c0 = node->left_first;
        uint32_t c1 = c0 + 1;
        float t0 = bvh__ray_aabb(ray, &bvh->nodes[c0].bounds);
        float t1 = bvh__ray_aabb(ray, &bvh->nodes[c1].bounds);

        if (t0 < t1) {
            if (t1 != FLT_MAX) { BVH_ASSERT(sp < BVH__TRAVERSE_STACK); stack[sp++] = c1; }
            if (t0 != FLT_MAX) { node_idx = c0; continue; }
        } else {
            if (t0 != FLT_MAX) { BVH_ASSERT(sp < BVH__TRAVERSE_STACK); stack[sp++] = c0; }
            if (t1 != FLT_MAX) { node_idx = c1; continue; }
        }
        if (sp == 0) return;
        node_idx = stack[--sp];
    }
}

/* ---- AABB / point queries ---- */

void bvh_aabb_query(const bvh_t* bvh, const bvh_aabb* box,
                    bvh_aabb_cb cb, void* user) {
    if (!bvh || bvh->node_count == 0) return;
    if (!bvh_aabb_overlap(&bvh->nodes[0].bounds, box)) return;

    uint32_t stack[BVH__TRAVERSE_STACK];
    int sp = 0;
    stack[sp++] = 0;

    while (sp) {
        const bvh_node* node = &bvh->nodes[stack[--sp]];
        if (node->prim_count > 0) {
            uint32_t s = node->left_first;
            for (uint32_t i = 0; i < node->prim_count; ++i) {
                uint32_t pi = bvh->prim_idx[s + i];
                if (bvh_aabb_overlap(&bvh->prim_aabbs[pi], box)) {
                    if (cb(pi, user)) return;
                }
            }
            continue;
        }
        uint32_t c0 = node->left_first, c1 = c0 + 1;
        if (bvh_aabb_overlap(&bvh->nodes[c0].bounds, box)) {
            BVH_ASSERT(sp < BVH__TRAVERSE_STACK); stack[sp++] = c0;
        }
        if (bvh_aabb_overlap(&bvh->nodes[c1].bounds, box)) {
            BVH_ASSERT(sp < BVH__TRAVERSE_STACK); stack[sp++] = c1;
        }
    }
}

static inline int bvh__aabb_contains_pt(const bvh_aabb* a, const float p[3]) {
    return (a->min[0] <= p[0]) & (p[0] <= a->max[0]) &
           (a->min[1] <= p[1]) & (p[1] <= a->max[1]) &
           (a->min[2] <= p[2]) & (p[2] <= a->max[2]);
}

void bvh_point_query(const bvh_t* bvh, const float point[3],
                     bvh_aabb_cb cb, void* user) {
    if (!bvh || bvh->node_count == 0) return;
    if (!bvh__aabb_contains_pt(&bvh->nodes[0].bounds, point)) return;

    uint32_t stack[BVH__TRAVERSE_STACK];
    int sp = 0;
    stack[sp++] = 0;

    while (sp) {
        const bvh_node* node = &bvh->nodes[stack[--sp]];
        if (node->prim_count > 0) {
            uint32_t s = node->left_first;
            for (uint32_t i = 0; i < node->prim_count; ++i) {
                uint32_t pi = bvh->prim_idx[s + i];
                if (bvh__aabb_contains_pt(&bvh->prim_aabbs[pi], point)) {
                    if (cb(pi, user)) return;
                }
            }
            continue;
        }
        uint32_t c0 = node->left_first, c1 = c0 + 1;
        if (bvh__aabb_contains_pt(&bvh->nodes[c0].bounds, point)) {
            BVH_ASSERT(sp < BVH__TRAVERSE_STACK); stack[sp++] = c0;
        }
        if (bvh__aabb_contains_pt(&bvh->nodes[c1].bounds, point)) {
            BVH_ASSERT(sp < BVH__TRAVERSE_STACK); stack[sp++] = c1;
        }
    }
}

/* ---- Pairwise / self overlap ---- */

/* Cross-overlap of node `na` in `a` with node `nb` in `b`. The two
 * subtrees must be disjoint (they always are when called from
 * bvh_overlap_query on different trees, or from bvh_self_overlap
 * across the two children of an internal node). */
static int bvh__cross_overlap(const bvh_t* a, uint32_t na,
                              const bvh_t* b, uint32_t nb,
                              bvh_pair_cb cb, void* user) {
    const bvh_node* A = &a->nodes[na];
    const bvh_node* B = &b->nodes[nb];
    if (!bvh_aabb_overlap(&A->bounds, &B->bounds)) return 0;

    int a_leaf = (A->prim_count > 0);
    int b_leaf = (B->prim_count > 0);

    if (a_leaf && b_leaf) {
        uint32_t sa = A->left_first, ca = A->prim_count;
        uint32_t sb = B->left_first, cb_ = B->prim_count;
        for (uint32_t i = 0; i < ca; ++i) {
            uint32_t pa = a->prim_idx[sa + i];
            const bvh_aabb* pab = &a->prim_aabbs[pa];
            for (uint32_t j = 0; j < cb_; ++j) {
                uint32_t pb = b->prim_idx[sb + j];
                if (bvh_aabb_overlap(pab, &b->prim_aabbs[pb])) {
                    if (cb(pa, pb, user)) return 1;
                }
            }
        }
        return 0;
    }

    /* Descend the larger node so the recursion shrinks both sides
     * roughly evenly. */
    int descend_a;
    if (a_leaf)      descend_a = 0;
    else if (b_leaf) descend_a = 1;
    else descend_a = bvh__aabb_half_area(&A->bounds) > bvh__aabb_half_area(&B->bounds);

    if (descend_a) {
        uint32_t c0 = A->left_first, c1 = c0 + 1;
        if (bvh__cross_overlap(a, c0, b, nb, cb, user)) return 1;
        if (bvh__cross_overlap(a, c1, b, nb, cb, user)) return 1;
    } else {
        uint32_t c0 = B->left_first, c1 = c0 + 1;
        if (bvh__cross_overlap(a, na, b, c0, cb, user)) return 1;
        if (bvh__cross_overlap(a, na, b, c1, cb, user)) return 1;
    }
    return 0;
}

void bvh_overlap_query(const bvh_t* bvh_a, const bvh_t* bvh_b,
                       bvh_pair_cb cb, void* user) {
    if (!bvh_a || !bvh_b || bvh_a->node_count == 0 || bvh_b->node_count == 0) return;
    bvh__cross_overlap(bvh_a, 0, bvh_b, 0, cb, user);
}

static int bvh__self_overlap(const bvh_t* bvh, uint32_t n,
                             bvh_pair_cb cb, void* user) {
    const bvh_node* node = &bvh->nodes[n];
    if (node->prim_count > 0) {
        uint32_t s = node->left_first;
        uint32_t c = node->prim_count;
        for (uint32_t i = 0; i < c; ++i) {
            uint32_t pa = bvh->prim_idx[s + i];
            const bvh_aabb* pab = &bvh->prim_aabbs[pa];
            for (uint32_t j = i + 1; j < c; ++j) {
                uint32_t pb = bvh->prim_idx[s + j];
                if (bvh_aabb_overlap(pab, &bvh->prim_aabbs[pb])) {
                    if (cb(pa, pb, user)) return 1;
                }
            }
        }
        return 0;
    }
    uint32_t c0 = node->left_first, c1 = c0 + 1;
    if (bvh__self_overlap(bvh, c0, cb, user)) return 1;
    if (bvh__self_overlap(bvh, c1, cb, user)) return 1;
    if (bvh__cross_overlap(bvh, c0, bvh, c1, cb, user)) return 1;
    return 0;
}

void bvh_self_overlap(const bvh_t* bvh, bvh_pair_cb cb, void* user) {
    if (!bvh || bvh->node_count == 0) return;
    bvh__self_overlap(bvh, 0, cb, user);
}

/* ---- Triangle helpers ---- */

void bvh_triangle_aabb(const float v0[3], const float v1[3], const float v2[3],
                       bvh_aabb* out) {
    for (int i = 0; i < 3; ++i) {
        float lo = v0[i], hi = v0[i];
        if (v1[i] < lo) lo = v1[i];
        if (v1[i] > hi) hi = v1[i];
        if (v2[i] < lo) lo = v2[i];
        if (v2[i] > hi) hi = v2[i];
        out->min[i] = lo;
        out->max[i] = hi;
    }
}

int bvh_ray_triangle(const bvh_ray* ray,
                     const float v0[3], const float v1[3], const float v2[3],
                     float* out_t, float* out_u, float* out_v) {
    const float EPS = 1e-8f;
    float e1[3] = { v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2] };
    float e2[3] = { v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2] };
    float p[3]  = { ray->dir[1]*e2[2] - ray->dir[2]*e2[1],
                    ray->dir[2]*e2[0] - ray->dir[0]*e2[2],
                    ray->dir[0]*e2[1] - ray->dir[1]*e2[0] };
    float det = e1[0]*p[0] + e1[1]*p[1] + e1[2]*p[2];
    if (det > -EPS && det < EPS) return 0;
    float inv = 1.0f / det;
    float s[3] = { ray->origin[0]-v0[0], ray->origin[1]-v0[1], ray->origin[2]-v0[2] };
    float u = (s[0]*p[0] + s[1]*p[1] + s[2]*p[2]) * inv;
    if (u < 0.0f || u > 1.0f) return 0;
    float q[3] = { s[1]*e1[2] - s[2]*e1[1],
                   s[2]*e1[0] - s[0]*e1[2],
                   s[0]*e1[1] - s[1]*e1[0] };
    float v = (ray->dir[0]*q[0] + ray->dir[1]*q[1] + ray->dir[2]*q[2]) * inv;
    if (v < 0.0f || u + v > 1.0f) return 0;
    float t = (e2[0]*q[0] + e2[1]*q[1] + e2[2]*q[2]) * inv;
    if (t <= ray->tmin || t >= ray->tmax) return 0;
    if (out_t) *out_t = t;
    if (out_u) *out_u = u;
    if (out_v) *out_v = v;
    return 1;
}

#endif /* BVH_IMPLEMENTATION */

#endif /* BVH_H_INCLUDED */

/* ============================================================
 *  License (0BSD)
 * ============================================================
 *
 * Copyright (c) 2026
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

