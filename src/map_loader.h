/* map_loader.h - single-header pure C Quake / TrenchBroom .map loader
 *
 * Usage
 * -----
 *   In ONE translation unit:
 *       #define MAP_LOADER_IMPLEMENTATION
 *       #include "map_loader.h"
 *
 *   In all other translation units:
 *       #include "map_loader.h"
 *
 * Build options can be customized before including the implementation:
 *       #define MAP_ASSERT(x)     my_assert(x)
 *       #define MAP_MALLOC(sz)    my_malloc(sz)
 *       #define MAP_FREE(p)       my_free(p)
 *
 * Winding convention of the three points listed per face. The default
 * is Quake / Z-up (CW from outside); some Y-up exports list points
 * the opposite way and need:
 *       #define MAP_WINDING_CCW
 *
 * The library has no external dependencies beyond the C standard library
 * and is C99 compatible.
 *
 * Format
 * ------
 * Loads "Standard" .map files such as those produced by TrenchBroom:
 *
 *     // entity 0
 *     {
 *     "classname" "worldspawn"
 *     // brush 0
 *     {
 *     ( x y z ) ( x y z ) ( x y z ) texname offU offV rot scaleU scaleV
 *     ...
 *     }
 *     ...
 *     }
 *
 * Each brush face is a half-space defined by three points; the brush is
 * the intersection of those half-spaces. The loader parses the file and
 * then converts each brush to a polygon-soup mesh by intersecting all
 * triples of face planes, keeping points that satisfy every other plane,
 * and grouping per-face vertices in winding order.
 *
 * Example
 * -------
 *
 *     #define MAP_LOADER_IMPLEMENTATION
 *     #include "map_loader.h"
 *
 *     int main(void) {
 *         map_t m;
 *         if (map_load(&m, "asteroid.map") != 0) return 1;
 *
 *         for (uint32_t e = 0; e < m.entity_count; ++e) {
 *             const map_entity* ent = &m.entities[e];
 *             const char* cls = map_entity_get(ent, "classname");
 *             for (uint32_t b = 0; b < ent->brush_count; ++b) {
 *                 typedef struct { float pos[3]; float norm[3]; } vertex;
 *                 map_vertex_layout L = {
 *                     .stride = sizeof(vertex),
 *                     .pos    = MAP_CONTIG3(offsetof(vertex, pos)),
 *                     .norm   = MAP_CONTIG3(offsetof(vertex, norm)),
 *                 };
 *                 uint32_t tri_count;
 *                 map_brush_triangulate(&ent->brushes[b], NULL, &L, &tri_count);
 *                 vertex* verts = malloc(sizeof(vertex) * 3 * tri_count);
 *                 map_brush_triangulate(&ent->brushes[b], verts, &L, &tri_count);
 *                 // upload verts (3 * tri_count vertices) to GPU ...
 *                 free(verts);
 *             }
 *         }
 *
 *         map_free(&m);
 *     }
 */

#ifndef MAP_LOADER_H_INCLUDED
#define MAP_LOADER_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_TEXTURE_NAME_MAX 64

/* ============================================================
 *  Types
 * ============================================================ */

typedef struct {
    /* Raw plane definition as it appeared in the file. */
    float points[3][3];

    /* Derived plane equation: outward-pointing normal, with
     *   normal . p == dist
     * for any p on the plane; the brush interior satisfies
     *   normal . p - dist <= 0
     * for every face. */
    float normal[3];
    float dist;

    /* Texture / UV info. tex_scale of 0 means "not set". */
    char  texture[MAP_TEXTURE_NAME_MAX];
    float tex_offset[2];
    float tex_rotation;
    float tex_scale[2];

    /* Range into map_brush::indices that holds this face's polygon,
     * ordered counter-clockwise around `normal`. Populated by
     * map_load. May be 0 if a brush is degenerate. */
    uint32_t first_index;
    uint32_t index_count;
} map_face;

typedef struct {
    map_face* faces;
    uint32_t  face_count;

    /* Deduplicated vertex positions used by this brush. */
    float    (*vertices)[3];
    uint32_t  vertex_count;

    /* Concatenated polygon indices for every face. Use the
     * (first_index, index_count) range on each face to slice. */
    uint32_t* indices;
    uint32_t  index_count;
} map_brush;

typedef struct {
    /* Parallel key/value arrays. Both strings are null-terminated and
     * owned by the map; they are freed with map_free. */
    char**     keys;
    char**     values;
    uint32_t   kv_count;

    map_brush* brushes;
    uint32_t   brush_count;
} map_entity;

typedef struct {
    map_entity* entities;
    uint32_t    entity_count;
} map_t;

/* ============================================================
 *  Loading
 * ============================================================ */

/* Parse a .map file from disk. `m` is overwritten and any prior
 * contents are leaked, so pass a zero-initialized struct or call
 * map_free first.
 *
 * Returns 0 on success, non-zero on I/O error, parse error, or
 * allocation failure. On failure `m` is fully released. */
int  map_load(map_t* m, const char* path);

/* Same, parsing from an in-memory buffer. The buffer does not need to
 * be null-terminated; `size` is authoritative. */
int  map_load_from_memory(map_t* m, const char* data, size_t size);

/* Release all storage owned by `m` and zero it. Safe to call on a
 * zero-initialized struct or on the same map twice. */
void map_free(map_t* m);

/* ============================================================
 *  Helpers
 * ============================================================ */

/* Look up a key in an entity's property table. Returns NULL if the
 * key is absent. The returned pointer is valid until map_free. */
const char* map_entity_get(const map_entity* e, const char* key);

/* Sentinel: store in pos[0] (resp. norm[0]) to skip that attribute. */
#define MAP_NO_OFFSET ((size_t)-1)

/* Vertex layout descriptor for map_brush_triangulate.
 *
 *   stride : byte distance between consecutive vertices.
 *   pos    : per-component byte offsets within each vertex of the
 *            position's x, y, z. Set pos[0] = MAP_NO_OFFSET to skip
 *            writing positions.
 *   norm   : same, for the per-face flat normal. Set norm[0] =
 *            MAP_NO_OFFSET to skip writing normals.
 *
 * Per-component offsets allow targeting unusual layouts (planar
 * components, padded floats, etc.); the typical case is contiguous
 * { x, y, z } fields where the three offsets differ by sizeof(float). */
typedef struct {
    size_t stride;
    size_t pos[3];
    size_t norm[3];
} map_vertex_layout;

/* Convenience: build a layout assuming each attribute occupies three
 * contiguous floats starting at the given base offset. Use
 * MAP_NO_OFFSET for `base` to skip the attribute. */
#define MAP_CONTIG3(base) \
    { (base), (base) + sizeof(float), (base) + 2 * sizeof(float) }

/* Triangulate every face of a brush, writing position and/or per-face
 * normal into a caller-provided vertex buffer at the offsets described
 * by `layout`. Normals are flat per-face: all three vertices of a
 * triangle receive the face's outward-pointing normal.
 *
 * Two-pass usage: call once with `out_buf == NULL` to populate
 * `*out_tri_count` (in this size-query mode `layout` may be NULL),
 * allocate room for 3 * tri_count vertices at the chosen stride, then
 * call again with that buffer to fill it.
 *
 * Each face is fan-triangulated from its first vertex; output winding
 * matches the face winding (CCW around the outward normal).
 *
 * Returns 0 on success. */
int  map_brush_triangulate(const map_brush* b,
                           void* out_buf,
                           const map_vertex_layout* layout,
                           uint32_t* out_tri_count);

/* Compute the axis-aligned bounding box of a single brush's vertices.
 * If the brush is empty, returns 0 and writes an inverted box
 * (out_min[i] = +FLT_MAX, out_max[i] = -FLT_MAX) so the result can
 * still be safely grown by subsequent inserts. Returns 1 otherwise. */
int  map_brush_aabb(const map_brush* b, float out_min[3], float out_max[3]);

/* Compute the axis-aligned bounding box of every brush vertex in the
 * map. Same empty-set semantics as map_brush_aabb. */
int  map_aabb(const map_t* m, float out_min[3], float out_max[3]);

#ifdef __cplusplus
}
#endif

/* ============================================================
 *  Implementation
 * ============================================================ */
#ifdef MAP_LOADER_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

#ifndef MAP_ASSERT
#  include <assert.h>
#  define MAP_ASSERT(x) assert(x)
#endif
#ifndef MAP_MALLOC
#  define MAP_MALLOC(sz) malloc(sz)
#endif
#ifndef MAP_FREE
#  define MAP_FREE(p)    free(p)
#endif

/* Distance tolerance for "vertex lies on plane" / "two vertices coincide".
 * Brush coordinates in TrenchBroom run from sub-unit (asteroid.map) up
 * to several hundred units (station.map); 1e-3 covers both reliably. */
#define MAP__EPS 1e-3f

/* ---- Lexer ---- */

typedef struct {
    const char* p;
    const char* end;
} map__lex;

static void map__skip_ws(map__lex* l) {
    while (l->p < l->end) {
        char c = *l->p;
        if (c == '/' && l->p + 1 < l->end && l->p[1] == '/') {
            while (l->p < l->end && *l->p != '\n') l->p++;
        } else if ((unsigned char)c <= ' ') {
            l->p++;
        } else break;
    }
}

static int map__peek(map__lex* l) {
    map__skip_ws(l);
    return (l->p < l->end) ? (unsigned char)*l->p : -1;
}

static int map__match(map__lex* l, char c) {
    map__skip_ws(l);
    if (l->p < l->end && *l->p == c) { l->p++; return 1; }
    return 0;
}

static int map__read_quoted(map__lex* l, char** out_dup) {
    map__skip_ws(l);
    if (l->p >= l->end || *l->p != '"') return 0;
    l->p++;
    const char* s = l->p;
    while (l->p < l->end && *l->p != '"') l->p++;
    size_t n = (size_t)(l->p - s);
    char* d = (char*)MAP_MALLOC(n + 1);
    if (!d) return 0;
    memcpy(d, s, n);
    d[n] = 0;
    if (l->p < l->end && *l->p == '"') l->p++;
    *out_dup = d;
    return 1;
}

static int map__read_word(map__lex* l, char* out, size_t cap) {
    map__skip_ws(l);
    size_t i = 0;
    while (l->p < l->end) {
        unsigned char c = (unsigned char)*l->p;
        if (c <= ' ' || c == '"' || c == '(' || c == ')' ||
            c == '{' || c == '}') break;
        if (i + 1 < cap) out[i++] = (char)c;
        l->p++;
    }
    if (cap > 0) out[i < cap ? i : cap - 1] = 0;
    return i > 0;
}

static int map__read_float(map__lex* l, float* out) {
    map__skip_ws(l);
    if (l->p >= l->end) return 0;
    char* endp = NULL;
    /* strtof needs a null-terminated buffer; we have a raw range. Copy
     * the next token into a small scratch buffer first. */
    char buf[64];
    size_t i = 0;
    const char* s = l->p;
    while (s < l->end) {
        unsigned char c = (unsigned char)*s;
        if (c <= ' ' || c == '(' || c == ')') break;
        if (i + 1 >= sizeof(buf)) return 0;
        buf[i++] = (char)c;
        s++;
    }
    if (i == 0) return 0;
    buf[i] = 0;
    *out = strtof(buf, &endp);
    if (endp == buf) return 0;
    l->p = s;
    return 1;
}

/* ---- Plane derivation ---- */

static int map__plane_from_points(float p[3][3],
                                  float out_n[3], float* out_d) {
    /* Quake convention (default): points are listed clockwise as seen
     * from outside the brush, so the outward normal is cross(p2-p0,
     * p1-p0). With MAP_WINDING_CCW the points are listed CCW from
     * outside instead, so we cross the other way. */
    float v1[3] = { p[1][0]-p[0][0], p[1][1]-p[0][1], p[1][2]-p[0][2] };
    float v2[3] = { p[2][0]-p[0][0], p[2][1]-p[0][1], p[2][2]-p[0][2] };
#ifdef MAP_WINDING_CCW
    float n[3] = {
        v1[1]*v2[2] - v1[2]*v2[1],
        v1[2]*v2[0] - v1[0]*v2[2],
        v1[0]*v2[1] - v1[1]*v2[0]
    };
#else
    float n[3] = {
        v2[1]*v1[2] - v2[2]*v1[1],
        v2[2]*v1[0] - v2[0]*v1[2],
        v2[0]*v1[1] - v2[1]*v1[0]
    };
#endif
    float len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (len < 1e-12f) return 0;
    float inv = 1.0f / len;
    out_n[0] = n[0]*inv; out_n[1] = n[1]*inv; out_n[2] = n[2]*inv;
    *out_d = out_n[0]*p[0][0] + out_n[1]*p[0][1] + out_n[2]*p[0][2];
    return 1;
}

/* Three-plane intersection via Cramer's rule. */
static int map__plane_intersect(const float n1[3], float d1,
                                const float n2[3], float d2,
                                const float n3[3], float d3,
                                float out[3]) {
    float c23[3] = { n2[1]*n3[2] - n2[2]*n3[1],
                     n2[2]*n3[0] - n2[0]*n3[2],
                     n2[0]*n3[1] - n2[1]*n3[0] };
    float det = n1[0]*c23[0] + n1[1]*c23[1] + n1[2]*c23[2];
    if (fabsf(det) < 1e-7f) return 0;
    float c31[3] = { n3[1]*n1[2] - n3[2]*n1[1],
                     n3[2]*n1[0] - n3[0]*n1[2],
                     n3[0]*n1[1] - n3[1]*n1[0] };
    float c12[3] = { n1[1]*n2[2] - n1[2]*n2[1],
                     n1[2]*n2[0] - n1[0]*n2[2],
                     n1[0]*n2[1] - n1[1]*n2[0] };
    float inv = 1.0f / det;
    out[0] = (d1*c23[0] + d2*c31[0] + d3*c12[0]) * inv;
    out[1] = (d1*c23[1] + d2*c31[1] + d3*c12[1]) * inv;
    out[2] = (d1*c23[2] + d2*c31[2] + d3*c12[2]) * inv;
    return 1;
}

static void map__face_basis(const float n[3], float u[3], float v[3]) {
    /* Pick the world axis least aligned with n as a seed. */
    float ax = fabsf(n[0]), ay = fabsf(n[1]), az = fabsf(n[2]);
    float a[3] = {0,0,0};
    if (ax <= ay && ax <= az)      a[0] = 1.0f;
    else if (ay <= az)              a[1] = 1.0f;
    else                            a[2] = 1.0f;
    /* u = normalize(cross(a, n)) */
    u[0] = a[1]*n[2] - a[2]*n[1];
    u[1] = a[2]*n[0] - a[0]*n[2];
    u[2] = a[0]*n[1] - a[1]*n[0];
    float lu = sqrtf(u[0]*u[0] + u[1]*u[1] + u[2]*u[2]);
    if (lu < 1e-12f) { u[0]=1; u[1]=0; u[2]=0; lu = 1.0f; }
    u[0]/=lu; u[1]/=lu; u[2]/=lu;
    /* v = cross(n, u) — already unit if n and u are unit and orthogonal. */
    v[0] = n[1]*u[2] - n[2]*u[1];
    v[1] = n[2]*u[0] - n[0]*u[2];
    v[2] = n[0]*u[1] - n[1]*u[0];
}

/* ---- Growable arrays ----
 *
 * MAP__GROW takes the byte size of one element rather than its type so it
 * can grow array-typed buffers (e.g. float[3]) for which `T*` would be a
 * malformed type expression. */

typedef float map__vec3[3];

#define MAP__GROW(arr, cap, elem_size) do {                                 \
        size_t _ncap = (cap) ? (cap) * 2 : 8;                               \
        void*  _np   = MAP_MALLOC((elem_size) * _ncap);                     \
        if (!_np) goto fail;                                                \
        if (cap) memcpy(_np, (arr), (elem_size) * (cap));                   \
        if (arr) MAP_FREE(arr);                                             \
        *(void**)&(arr) = _np; (cap) = (uint32_t)_ncap;                     \
    } while (0)

/* ---- Brush geometry build ---- */

static int map__build_brush(map_brush* b) {
    b->vertices = NULL;
    b->vertex_count = 0;
    b->indices = NULL;
    b->index_count = 0;
    if (b->face_count < 4) return 0; /* leave the brush empty */

    /* Generate candidate vertices from every triple of face planes. */
    float (*cand)[3] = NULL;
    uint32_t cand_n = 0, cand_cap = 0;

    for (uint32_t i = 0; i < b->face_count; ++i)
    for (uint32_t j = i + 1; j < b->face_count; ++j)
    for (uint32_t k = j + 1; k < b->face_count; ++k) {
        float p[3];
        if (!map__plane_intersect(b->faces[i].normal, b->faces[i].dist,
                                  b->faces[j].normal, b->faces[j].dist,
                                  b->faces[k].normal, b->faces[k].dist, p))
            continue;

        int inside = 1;
        for (uint32_t m = 0; m < b->face_count; ++m) {
            if (m == i || m == j || m == k) continue;
            const map_face* f = &b->faces[m];
            float d = f->normal[0]*p[0] + f->normal[1]*p[1] +
                      f->normal[2]*p[2] - f->dist;
            if (d > MAP__EPS) { inside = 0; break; }
        }
        if (!inside) continue;

        int dup = 0;
        for (uint32_t v = 0; v < cand_n; ++v) {
            float dx = cand[v][0] - p[0];
            float dy = cand[v][1] - p[1];
            float dz = cand[v][2] - p[2];
            if (dx*dx + dy*dy + dz*dz < MAP__EPS*MAP__EPS) { dup = 1; break; }
        }
        if (dup) continue;

        if (cand_n == cand_cap) MAP__GROW(cand, cand_cap, sizeof(map__vec3));
        cand[cand_n][0] = p[0];
        cand[cand_n][1] = p[1];
        cand[cand_n][2] = p[2];
        cand_n++;
    }

    if (cand_n < 4) {
        if (cand) MAP_FREE(cand);
        return 0;
    }

    b->vertices = (float(*)[3])MAP_MALLOC(sizeof(float) * 3 * cand_n);
    if (!b->vertices) goto fail;
    memcpy(b->vertices, cand, sizeof(float) * 3 * cand_n);
    b->vertex_count = cand_n;
    MAP_FREE(cand);
    cand = NULL;

    /* For each face, gather the brush vertices that lie on its plane. */
    uint32_t* per_face = (uint32_t*)MAP_MALLOC(
        sizeof(uint32_t) * b->face_count * b->vertex_count);
    uint32_t* per_face_n = (uint32_t*)MAP_MALLOC(
        sizeof(uint32_t) * b->face_count);
    if (!per_face || !per_face_n) {
        if (per_face)   MAP_FREE(per_face);
        if (per_face_n) MAP_FREE(per_face_n);
        goto fail;
    }

    uint32_t total = 0;
    for (uint32_t f = 0; f < b->face_count; ++f) {
        const map_face* face = &b->faces[f];
        uint32_t* row = &per_face[f * b->vertex_count];
        uint32_t n = 0;
        for (uint32_t v = 0; v < b->vertex_count; ++v) {
            float d = face->normal[0]*b->vertices[v][0] +
                      face->normal[1]*b->vertices[v][1] +
                      face->normal[2]*b->vertices[v][2] - face->dist;
            if (fabsf(d) < MAP__EPS) row[n++] = v;
        }
        per_face_n[f] = n;
        total += n;
    }

    b->indices = (uint32_t*)MAP_MALLOC(sizeof(uint32_t) * total);
    if (!b->indices) { MAP_FREE(per_face); MAP_FREE(per_face_n); goto fail; }
    b->index_count = total;

    uint32_t cursor = 0;
    for (uint32_t f = 0; f < b->face_count; ++f) {
        map_face* face = &b->faces[f];
        uint32_t  n    = per_face_n[f];
        uint32_t* row  = &per_face[f * b->vertex_count];

        face->first_index = cursor;
        face->index_count = n;

        if (n < 3) {
            for (uint32_t i = 0; i < n; ++i) b->indices[cursor++] = row[i];
            continue;
        }

        /* Centroid */
        float c[3] = {0,0,0};
        for (uint32_t i = 0; i < n; ++i) {
            c[0] += b->vertices[row[i]][0];
            c[1] += b->vertices[row[i]][1];
            c[2] += b->vertices[row[i]][2];
        }
        c[0] /= (float)n; c[1] /= (float)n; c[2] /= (float)n;

        float u[3], v[3];
        map__face_basis(face->normal, u, v);

        /* Selection sort by polar angle around the face normal. n is
         * tiny (typically 3-8) so the O(n^2) loop is irrelevant. */
        for (uint32_t i = 0; i + 1 < n; ++i) {
            uint32_t a   = row[i];
            float ax     = (b->vertices[a][0]-c[0])*u[0] +
                           (b->vertices[a][1]-c[1])*u[1] +
                           (b->vertices[a][2]-c[2])*u[2];
            float ay     = (b->vertices[a][0]-c[0])*v[0] +
                           (b->vertices[a][1]-c[1])*v[1] +
                           (b->vertices[a][2]-c[2])*v[2];
            float a_ang  = atan2f(ay, ax);
            for (uint32_t j = i + 1; j < n; ++j) {
                uint32_t bb = row[j];
                float bx    = (b->vertices[bb][0]-c[0])*u[0] +
                              (b->vertices[bb][1]-c[1])*u[1] +
                              (b->vertices[bb][2]-c[2])*u[2];
                float by    = (b->vertices[bb][0]-c[0])*v[0] +
                              (b->vertices[bb][1]-c[1])*v[1] +
                              (b->vertices[bb][2]-c[2])*v[2];
                float b_ang = atan2f(by, bx);
                if (b_ang < a_ang) {
                    uint32_t t = row[i]; row[i] = row[j]; row[j] = t;
                    a_ang = b_ang;
                }
            }
        }

        for (uint32_t i = 0; i < n; ++i) b->indices[cursor++] = row[i];
    }

    MAP_FREE(per_face);
    MAP_FREE(per_face_n);
    return 0;

fail:
    if (cand)        MAP_FREE(cand);
    if (b->vertices) { MAP_FREE(b->vertices); b->vertices = NULL; b->vertex_count = 0; }
    if (b->indices)  { MAP_FREE(b->indices);  b->indices  = NULL; b->index_count  = 0; }
    return 1;
}

/* ---- Parsing ---- */

static int map__parse_face(map__lex* l, map_face* f) {
    memset(f, 0, sizeof(*f));

    for (int p = 0; p < 3; ++p) {
        if (!map__match(l, '(')) return 1;
        if (!map__read_float(l, &f->points[p][0])) return 1;
        if (!map__read_float(l, &f->points[p][1])) return 1;
        if (!map__read_float(l, &f->points[p][2])) return 1;
        if (!map__match(l, ')')) return 1;
    }

    if (!map__read_word(l, f->texture, sizeof(f->texture))) return 1;

    /* Standard format: 5 floats (offU offV rot scaleU scaleV). */
    if (!map__read_float(l, &f->tex_offset[0]))   return 1;
    if (!map__read_float(l, &f->tex_offset[1]))   return 1;
    if (!map__read_float(l, &f->tex_rotation))    return 1;
    if (!map__read_float(l, &f->tex_scale[0]))    return 1;
    if (!map__read_float(l, &f->tex_scale[1]))    return 1;

    if (!map__plane_from_points(f->points, f->normal, &f->dist)) return 1;
    return 0;
}

static int map__parse_brush(map__lex* l, map_brush* b) {
    memset(b, 0, sizeof(*b));
    if (!map__match(l, '{')) return 1;

    map_face* faces = NULL;
    uint32_t cap = 0;

    for (;;) {
        int c = map__peek(l);
        if (c == '}') { l->p++; break; }
        if (c < 0) goto fail;

        if (b->face_count == cap) MAP__GROW(faces, cap, sizeof(map_face));
        if (map__parse_face(l, &faces[b->face_count]) != 0) goto fail;
        b->face_count++;
    }

    b->faces = faces;
    if (map__build_brush(b) != 0) {
        /* keep faces; geometry build is best-effort */
    }
    return 0;

fail:
    if (faces) MAP_FREE(faces);
    return 1;
}

static int map__parse_entity(map__lex* l, map_entity* e) {
    memset(e, 0, sizeof(*e));
    if (!map__match(l, '{')) return 1;

    char**      keys    = NULL;
    char**      values  = NULL;
    map_brush*  brushes = NULL;
    uint32_t    kv_cap  = 0;
    uint32_t    br_cap  = 0;

    for (;;) {
        int c = map__peek(l);
        if (c == '}') { l->p++; break; }
        if (c < 0) goto fail;

        if (c == '"') {
            char* k = NULL; char* v = NULL;
            if (!map__read_quoted(l, &k)) goto fail;
            if (!map__read_quoted(l, &v)) { MAP_FREE(k); goto fail; }
            if (e->kv_count == kv_cap) {
                /* Grow keys and values together so the two arrays
                 * always share an identical valid range. */
                uint32_t kc = kv_cap;
                MAP__GROW(keys,   kc,     sizeof(char*));
                MAP__GROW(values, kv_cap, sizeof(char*));
                MAP_ASSERT(kc == kv_cap);
            }
            keys  [e->kv_count] = k;
            values[e->kv_count] = v;
            e->kv_count++;
        } else if (c == '{') {
            if (e->brush_count == br_cap) MAP__GROW(brushes, br_cap, sizeof(map_brush));
            if (map__parse_brush(l, &brushes[e->brush_count]) != 0) goto fail;
            e->brush_count++;
        } else {
            goto fail;
        }
    }

    e->keys    = keys;
    e->values  = values;
    e->brushes = brushes;
    return 0;

fail:
    if (keys) {
        for (uint32_t i = 0; i < e->kv_count; ++i) {
            if (keys[i])   MAP_FREE(keys[i]);
            if (values && values[i]) MAP_FREE(values[i]);
        }
        MAP_FREE(keys);
    }
    if (values) MAP_FREE(values);
    if (brushes) {
        for (uint32_t i = 0; i < e->brush_count; ++i) {
            if (brushes[i].faces)    MAP_FREE(brushes[i].faces);
            if (brushes[i].vertices) MAP_FREE(brushes[i].vertices);
            if (brushes[i].indices)  MAP_FREE(brushes[i].indices);
        }
        MAP_FREE(brushes);
    }
    return 1;
}

/* ---- Public API ---- */

int map_load_from_memory(map_t* m, const char* data, size_t size) {
    MAP_ASSERT(m);
    memset(m, 0, sizeof(*m));

    map__lex l;
    l.p   = data;
    l.end = data + size;

    map_entity* ents = NULL;
    uint32_t cap = 0;

    for (;;) {
        int c = map__peek(&l);
        if (c < 0) break;
        if (c != '{') goto fail;

        if (m->entity_count == cap) MAP__GROW(ents, cap, sizeof(map_entity));
        if (map__parse_entity(&l, &ents[m->entity_count]) != 0) goto fail;
        m->entity_count++;
    }

    m->entities = ents;
    return 0;

fail:
    if (ents) {
        for (uint32_t i = 0; i < m->entity_count; ++i) {
            map_entity* e = &ents[i];
            for (uint32_t k = 0; k < e->kv_count; ++k) {
                if (e->keys && e->keys[k])     MAP_FREE(e->keys[k]);
                if (e->values && e->values[k]) MAP_FREE(e->values[k]);
            }
            if (e->keys)   MAP_FREE(e->keys);
            if (e->values) MAP_FREE(e->values);
            for (uint32_t b = 0; b < e->brush_count; ++b) {
                map_brush* br = &e->brushes[b];
                if (br->faces)    MAP_FREE(br->faces);
                if (br->vertices) MAP_FREE(br->vertices);
                if (br->indices)  MAP_FREE(br->indices);
            }
            if (e->brushes) MAP_FREE(e->brushes);
        }
        MAP_FREE(ents);
    }
    memset(m, 0, sizeof(*m));
    return 1;
}

int map_load(map_t* m, const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return 1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 1; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return 1; }
    rewind(fp);
    char* buf = (char*)MAP_MALLOC((size_t)sz);
    if (!buf) { fclose(fp); return 1; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    if (got != (size_t)sz) { MAP_FREE(buf); return 1; }

    int rc = map_load_from_memory(m, buf, got);
    MAP_FREE(buf);
    return rc;
}

void map_free(map_t* m) {
    if (!m) return;
    for (uint32_t i = 0; i < m->entity_count; ++i) {
        map_entity* e = &m->entities[i];
        for (uint32_t k = 0; k < e->kv_count; ++k) {
            if (e->keys   && e->keys[k])   MAP_FREE(e->keys[k]);
            if (e->values && e->values[k]) MAP_FREE(e->values[k]);
        }
        if (e->keys)   MAP_FREE(e->keys);
        if (e->values) MAP_FREE(e->values);
        for (uint32_t b = 0; b < e->brush_count; ++b) {
            map_brush* br = &e->brushes[b];
            if (br->faces)    MAP_FREE(br->faces);
            if (br->vertices) MAP_FREE(br->vertices);
            if (br->indices)  MAP_FREE(br->indices);
        }
        if (e->brushes) MAP_FREE(e->brushes);
    }
    if (m->entities) MAP_FREE(m->entities);
    memset(m, 0, sizeof(*m));
}

const char* map_entity_get(const map_entity* e, const char* key) {
    if (!e || !key) return NULL;
    for (uint32_t i = 0; i < e->kv_count; ++i) {
        if (e->keys[i] && strcmp(e->keys[i], key) == 0) return e->values[i];
    }
    return NULL;
}

int map_brush_triangulate(const map_brush* b,
                          void* out_buf,
                          const map_vertex_layout* layout,
                          uint32_t* out_tri_count) {
    MAP_ASSERT(b && out_tri_count);

    uint32_t tri_count = 0;
    for (uint32_t f = 0; f < b->face_count; ++f) {
        if (b->faces[f].index_count >= 3)
            tri_count += b->faces[f].index_count - 2;
    }
    *out_tri_count = tri_count;
    if (!out_buf || tri_count == 0) return 0;

    MAP_ASSERT(layout);
    int write_pos  = (layout->pos[0]  != MAP_NO_OFFSET);
    int write_norm = (layout->norm[0] != MAP_NO_OFFSET);

    char* w = (char*)out_buf;
    for (uint32_t f = 0; f < b->face_count; ++f) {
        const map_face* face = &b->faces[f];
        if (face->index_count < 3) continue;
        const uint32_t* idx = &b->indices[face->first_index];
        const float*    n   = face->normal;
        const float*    v0  = b->vertices[idx[0]];

        for (uint32_t i = 1; i + 1 < face->index_count; ++i) {
            const float* v1 = b->vertices[idx[i]];
            const float* v2 = b->vertices[idx[i + 1]];
            const float* verts[3] = { v0, v1, v2 };
            for (int k = 0; k < 3; ++k) {
                if (write_pos) {
                    *(float*)(w + layout->pos[0]) = verts[k][0];
                    *(float*)(w + layout->pos[1]) = verts[k][1];
                    *(float*)(w + layout->pos[2]) = verts[k][2];
                }
                if (write_norm) {
                    *(float*)(w + layout->norm[0]) = n[0];
                    *(float*)(w + layout->norm[1]) = n[1];
                    *(float*)(w + layout->norm[2]) = n[2];
                }
                w += layout->stride;
            }
        }
    }

    return 0;
}

int map_brush_aabb(const map_brush* b, float out_min[3], float out_max[3]) {
    MAP_ASSERT(b && out_min && out_max);
    out_min[0] = out_min[1] = out_min[2] =  FLT_MAX;
    out_max[0] = out_max[1] = out_max[2] = -FLT_MAX;
    for (uint32_t v = 0; v < b->vertex_count; ++v) {
        const float* p = b->vertices[v];
        if (p[0] < out_min[0]) out_min[0] = p[0];
        if (p[1] < out_min[1]) out_min[1] = p[1];
        if (p[2] < out_min[2]) out_min[2] = p[2];
        if (p[0] > out_max[0]) out_max[0] = p[0];
        if (p[1] > out_max[1]) out_max[1] = p[1];
        if (p[2] > out_max[2]) out_max[2] = p[2];
    }
    return b->vertex_count > 0;
}

int map_aabb(const map_t* m, float out_min[3], float out_max[3]) {
    MAP_ASSERT(m && out_min && out_max);
    out_min[0] = out_min[1] = out_min[2] =  FLT_MAX;
    out_max[0] = out_max[1] = out_max[2] = -FLT_MAX;
    int seen = 0;
    for (uint32_t e = 0; e < m->entity_count; ++e) {
        const map_entity* ent = &m->entities[e];
        for (uint32_t b = 0; b < ent->brush_count; ++b) {
            float lo[3], hi[3];
            if (!map_brush_aabb(&ent->brushes[b], lo, hi)) continue;
            if (lo[0] < out_min[0]) out_min[0] = lo[0];
            if (lo[1] < out_min[1]) out_min[1] = lo[1];
            if (lo[2] < out_min[2]) out_min[2] = lo[2];
            if (hi[0] > out_max[0]) out_max[0] = hi[0];
            if (hi[1] > out_max[1]) out_max[1] = hi[1];
            if (hi[2] > out_max[2]) out_max[2] = hi[2];
            seen = 1;
        }
    }
    return seen;
}

#endif /* MAP_LOADER_IMPLEMENTATION */

#endif /* MAP_LOADER_H_INCLUDED */
