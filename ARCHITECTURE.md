# space-c — Architecture

This document is the canon for how `space-c` is built: the patterns that hold
across the codebase, the conventions to keep following, and the places where
exploration has left drift worth reconciling. It is meant to be read top to
bottom once, then used as a reference.

The codebase was grown by feel; this file is the retroactive description of the
shape it converged on. When a new decision conflicts with something here, either
follow this doc or change this doc — don't silently fork a third way.

---

## 1. The thesis

`space-c` is a **single-translation-unit, data-oriented C game**. It is built
from a small number of strong, repeated ideas:

- **Entities are array indices.** State lives in global Structure-of-Arrays
  tables; "systems" are loops over those tables.
- **Quake `.map` brushes are the universal geometry** — render mesh, collision
  hull, and gameplay metadata all come from one format.
- **The BVH is the universal spatial query** — collision, picking, nearest, and
  line-of-sight all route through it via callbacks.
- **The GPU reads one giant buffer by address** — bindless via buffer-device-
  address, indirect draws, almost no descriptor sets.
- **Everything is immediate-mode** — no retained scene graph, no deferred
  command objects; rebuild what you need each frame.

The style is *no-abstraction, table-driven, rebuild-every-frame*. New code
should feel boring in the same way the existing code is boring.

---

## 2. Build model: one translation unit, two kinds of header

`main.c` is the only compiland. The `Makefile` compiles exactly
`./src/main.c`; every other source file is a header `#include`d into it. There
is no separate linking of game objects.

There are two distinct kinds of header, and new code should consciously pick one:

### 2a. stb-style libraries

`bvh.h`, `map_loader.h`, vendored `nuklear.h`, vendored `cglm`.

- Include-guarded **public API at the top**, implementation gated behind
  `#define <LIB>_IMPLEMENTATION` (defined once, in `main.c`).
- Self-contained, documented, dependency-light (C stdlib only), reusable in
  another project unchanged.
- This is the *mature* layer. If a thing could live on GitHub by itself, it goes
  here.

`main.c` turns these on:

```c
#define MAP_LOADER_IMPLEMENTATION
#define MAP_WINDING_CCW
#include "./map_loader.h"
#define BVH_IMPLEMENTATION
#include "./bvh.h"
```

### 2b. in-place game modules

`entities.h`, `physics.h`, `ai.h`, `engine.h`, `contracts.h`, `faction.h`,
`ui.h`, `vlk.h`, `vlk_nk.h`.

- Guarded with `#ifndef GAME_<NAME>` but they define **globals and full function
  bodies directly in the header** — there is no implementation gate.
- They are "headers" in name only. They *are* the program, chaptered into
  TU-sized files. This is legal only because there is exactly one TU.
- Anything that touches the entity tables lives here.

**Rule:** reusable + standalone → stb-style. Touches the game's global tables →
in-place module.

### 2c. Include order is dependency order

The modules form a layered dependency graph. `main.c` includes them roughly
bottom-up:

```
map_loader.h  bvh.h            (foundation libs — no game knowledge)
        │        │
   entities.h  ───┘            (the SoA tables + lifecycle)
        │
   physics.h  contracts.h  faction.h
        │          │          │
        └────────  ai.h  ─────┘   (systems that read the tables)
                    │
                engine.h          (make_entity, sim_loop — the schedule)
                    │
       vlk.h ── vlk_nk.h ── ui.h  (rendering + UI read the tables)
                    │
                 main.c           (window, input, the frame loop)
```

Keep new modules in their layer. A foundation lib must never include a game
module.

> Known drift: `main.c` currently includes `engine.h` twice and `MAX_ENTITIES`
> is `#define`d in both `entities.h` and `engine.h`. Harmless under the guards,
> but `MAX_ENTITIES` should live in exactly one place (`entities.h`). Tracked in
> `issues/` (see §12).

---

## 3. Entities are indices; state is SoA tables ("ECS by convention")

There is **no entity struct**. An entity is a `uint32_t` index. All of its state
is spread across global parallel arrays, each named `SUBSYSTEM_FIELD`:

```c
mat4     ENTITY_TRANSFORM[MAX_ENTITIES];
vec3     ENTITY_INERTIA[MAX_ENTITIES];
int32_t  ENTITY_HEALTH[MAX_ENTITIES];
float    GUN_RELOAD[MAX_ENTITIES];
uint32_t AI_MODE[MAX_ENTITIES];
...
```

"Components" are added by adding another table and initializing it in the
lifecycle functions. "Systems" are free functions that loop over the live range
and read/write those tables.

### 3a. Lifecycle

- `make_entity(...)` → returns the next free slot and bumps `ENTITY_COUNT`
  (`engine.h`). It is the single place a slot is born; it sets the non-zero
  defaults for a fresh entity.
- `entity_copy(src)` → clones a slot field-by-field (used by asteroid splitting).
- **Nothing is ever freed.** "Death" is a state, not a deletion: `ENTITY_DEAD`
  holds a small animation timer, and deactivation is done by zeroing
  `ENTITY_MODEL` and `ENTITY_COLLIDER_GROUP`. There is no free list and no slot
  reuse; `ENTITY_COUNT` only grows.

When you add a component table, decide whether `make_entity` and `entity_copy`
need to touch it. If a fresh entity's correct default for that field is **0**,
you can rely on zero-initialization (see §5) and skip it — that is the common
case and why those functions only set the *non-zero* defaults.

### 3b. Sentinels: the optional-component vocabulary

Optionality is encoded with in-band sentinel values rather than presence bits.
This vocabulary is load-bearing and was previously only implicit. **The
canonical table:**

| Field(s)                         | Sentinel        | Meaning                                  |
|----------------------------------|-----------------|------------------------------------------|
| `ENTITY_MODEL`                   | `0`             | no model / not drawn / deactivated       |
| `ENTITY_COLLIDER_GROUP`          | `0`             | excluded from the world BVH (no collide) |
| `ENTITY_THRUST_POWER[i][0]`      | `-1`            | no linear engine (skip in thrusters)     |
| `ENTITY_VEC_THRUST`              | `-1`            | no rotational engine                     |
| `ENTITY_DEAD`                    | `0`             | alive; `> 0` is the death-anim timer     |
| `ENTITY_FACTION`                 | `0`             | "no faction" — AI skips these            |
| `ENTITY_FACTION`                 | `-1`            | bullets / neutral projectiles            |
| `AI_TARGETS`                     | `-1`            | no current target                        |
| `GUN_RELOAD_TIME`                | `0`             | entity has no gun                        |
| `CONTRACT_COUNT`                 | starts at `1`   | contract id `0` means "no contract"      |
| `ENTITY_*_CONTRACT` / inventory  | `0`             | empty slot                               |

**Rule:** when adding a new optional component, prefer `0` = absent so demand-
zero pages give you the right default for free. If `0` is a valid value, use
`-1` and document it in this table.

---

## 4. Index spaces and the null-slot convention

The codebase juggles several **distinct integer index spaces**. They are all
bare `uint32_t` / `int32_t` with no type distinction, so the *same numeric value*
means completely different things depending on which space it belongs to. This is
the single easiest place to introduce a silent bug — using a value from one space
to index a table belonging to another. Knowing the spaces, their reservations,
and the sanctioned bridges between them is essential.

### 4a. The spaces

| Space | Backing tables | Starts at | Created by | `0` means |
|-------|----------------|-----------|------------|-----------|
| **Entity id** | `ENTITY_*[]`, `GUN_*[]`, `AI_*[]`, … | 0 | `make_entity` / `entity_copy` | informally "null entity" (see §4c) |
| **Model id** | `GAME_MODELS[]`, `MODEL_HULLS[]`, `MODEL_MAP[]`; stored in `ENTITY_MODEL` | 1 | `load_model` | reserved — "no model" |
| **Faction id** | `FACTION_*[]`; stored in `ENTITY_FACTION` | 0 | `faction_create` | **not reserved** — collides (see §4c) |
| **Contract id** | `CONTRACT_*[]`; stored in `ENTITY_ACCEPTED_CONTRACT`, `ENTITY_CONTRACTS[][]` | 1 | `contract_add` | reserved — "no contract" |
| **World-BVH prim index** | `ENTITY_BVH` leaves; `AABB_IDS[]` | 0 | `build_entity_bvh` (per frame) | a real prim — *not* an entity id |
| **Model-hull BVH prim index** | `MODEL_HULLS[m]` leaves | 0 | `model_set_bvh` | brush index in `map->entities[0].brushes[]` |
| **Instance-buffer index** | `gl_InstanceIndex` into the per-frame instance buffer | 0 | `vlk_queueModelDrawCommands` | a real instance — *not* an entity id |
| **Map sub-entity index** | `map_t::entities[]` of a model | 0 | `map_load` | the worldspawn entity (brush geometry) |
| **Inventory slot** | `ENTITY_INVENTORY_*[e][]` | 0 | — | a real slot (`count == 0` = empty) |

The first three rows are *persistent identity*. The BVH/instance rows are
**ephemeral, compacted** spaces rebuilt every frame: the world BVH and the
instance buffer contain only *live, eligible* entities, densely packed, so a
prim/instance index is **never** equal to the entity id it represents.

### 4b. The bridges (the only sanctioned crossings)

Crossing between spaces happens *only* through these named mappings. Never index
a table with a raw value from another space.

- `AABB_IDS[prim_index]  → entity id`  — world-BVH prim back to entity. Every BVH
  callback (`handle_collision`, `ray_intersection_cb`, `ai_ship_ray_shoot_cb`,
  `target_asteroid_cb`) does this on its first line.
- `ENTITY_MODEL[entity_id] → model id`  — then `MODEL_HULLS[…]` / `MODEL_MAP[…]`
  index the model space.
- `ModelInstanceData.entity_id`  — instance index → entity id, carried as a
  field so the (unfinished) GPU id-buffer picking path can recover the entity.
- `ENTITY_DOCKING[ship] = { station entity id, station map-sub-entity index }`  —
  a *compound* id: a dock point is identified by *which station entity* plus
  *which `info_landingpad` sub-entity inside that station's model*.

### 4c. Reservations, and the slot-0 problem

Two spaces reserve index 0 cleanly by starting their counter at 1: **model 0**
("no model") and **contract 0** ("no contract"). That is the pattern to copy.

Two do **not**, and both are latent hazards:

- **Entity 0 is only informally null.** Several places treat entity id `0` as
  "none": `ray_intersection`'s `incident_entity` defaults to `0` and callers test
  `> 0` (`main.c`, `find_correction_location`); `CONTRACT_CLAIMING_ENTITYID != 0`
  means "claimed". This is correct *today only by luck of ordering* — `main.c`
  creates 10 non-colliding `DEBUG_MARKERS` first, so entities `0..9` never appear
  as a pick/collision/claim result. Nothing enforces that. If an entity that can
  be hit or can claim a contract ever lands in slot 0, "null" and "entity 0"
  become indistinguishable.
- **Faction 0 is overloaded.** `faction_create("Player")` returns `0`, so the
  player faction *is* faction 0 — but faction `0` is also the value world objects
  and debug markers are created with, and the value `ai_accept_contracts` treats
  as "skip this entity". So faction 0 simultaneously means *player*,
  *world/neutral*, and *don't-run-AI*. It works while the player's ships are all
  human-controlled; it breaks the moment you want an AI-driven player-faction
  ship. (`-1` is separately used for projectiles/neutral in `ENTITY_FACTION`.)

### 4d. Conventions to adopt

1. **Reserve index 0 = null in every id space.** Counts start at 1. Model and
   contract already do; bring entity and faction into line — make slot 0 a
   permanent inert "null entity", and create faction 0 as an explicit
   `"Neutral"`/world dummy so the player gets a real id ≥ 1.
2. **Name every variable and parameter by its space** — `entity_id`, `model_id`,
   `faction`, `contract_id`, `prim_index`, `instance_index`, `dock_point`. Do not
   call a BVH prim "entity"; the bug hides in the name.
3. **Cross spaces only through the §4b bridges.** A raw integer is never valid as
   an index into a table from a different space.
4. **Optional but cheap:** `typedef uint32_t entity_id;` (etc.) purely as
   documentation. C won't enforce it, but it makes signatures self-describing.

The faction-0 collision and the entity-0 fragility are tracked in `issues/`
(see §12).

---

## 5. The memory trick: demand-zero `.bss` is the allocator

`MAX_ENTITIES` is **2,000,000**, and there are ~30 tables keyed by it. Naively
that is hundreds of megabytes (e.g. `ENTITY_TRANSFORM` alone is
`64 B × 2M = 128 MB`). This is **intentional and effectively free on Linux**, and
it is a deliberate architectural choice, not an oversight:

- Zero-initialized globals live in `.bss`, which is **demand-zero paged**. No
  physical page is allocated until the first write to it, and the kernel
  guarantees the page is zeroed when it is faulted in.
- So the real cost is proportional to the *high-water mark of entities actually
  touched*, rounded up to pages — not to `MAX_ENTITIES`. A session with 5,000
  entities pays for ~5,000 entities, even though the array is sized for 2M.
- The same property is what makes the sentinel-`0` convention (§3b) sound: a
  never-before-touched slot reads back as all-zero, so "default = 0" components
  need no initialization at all.

This is why we can "punt the SoA sizing problem": pick a cap far larger than any
real session and let the kernel page in only what's used. Growing or compacting
the tables would add complexity for memory we are not actually spending.

**Caveats to keep in mind:**

- It relies on Linux overcommit + demand paging. On the **Windows (mingw)
  build**, the PE loader *commits* `.bss` at load, so the full size counts against
  the commit charge (physical pages are still backed lazily, but a constrained
  pagefile could refuse the commit). If the Windows target ever matters, that is
  the place this trick is weakest — revisit `MAX_ENTITIES` for that build, or
  switch the tables to a growable arena there.
- Touching a table sparsely (e.g. writing entity index 1,999,999 early)
  defeats the high-water-mark benefit by faulting in a page near the top.
  Allocation is monotonic from 0, so this doesn't happen in practice — keep it
  that way.
- `make_entity` must not `memset` whole tables across `MAX_ENTITIES`; that would
  fault in every page and throw the trick away. Initialize **only the new slot**.

---

## 6. The frame & the schedule

`main.c`'s `while (!done)` loop is, in order:

1. Compute `delta_t`, pump SDL events (also fed to nuklear via
   `vlk_nk_handleEvent`), handle resize / right-click picking / quit.
2. Translate mouse + keyboard into the player entity's thrust tables.
3. `sim_loop(delta_t)` — advance the whole simulation.
4. `vlk_queueModelDrawCommands(...)` / `vlk_queueFxDrawCommands(...)` — rebuild
   the GPU instance + indirect-draw buffers from the entity tables.
5. `ui_draw(window)` — build this frame's nuklear UI.
6. `vlk_beginDraw()` → `vlk_issueDraws()` → `vlk_nk_render()` → `vlk_endDraw()`.

### `sim_loop` is the system schedule of record

`engine.h`'s `sim_loop` is the single clearest expression of the architecture —
a hand-ordered list of system calls with no scheduler and no inter-system events,
just deterministic phases over shared tables:

```
factions:   ai_accept_contracts
AI:         ai_ship_select_tasks → ai_ship_thrusters → ai_ship_shoot
physics:    build_entity_bvh → apply_vec_thrusters → apply_thrusters
            → apply_movement → do_collisions
activities: collect_items → fire_guns → death_animations → dock_ships
fx:         play_fx
```

**Rule:** a "system" is a `void name(float delta_time)` (or argless) function
that loops `for (i = 0; i < ENTITY_COUNT; i++)` over the tables. You register it
by *calling it from `sim_loop`* in the correct phase. Ordering is meaningful
(e.g. `build_entity_bvh` must precede everything that queries the world BVH this
frame) — put new systems in the phase whose inputs they depend on.

---

## 7. Spatial queries: the BVH is the one true broadphase

`bvh.h` is a polished standalone SAH-binned BVH. The game uses it at two tiers:

- **Model hulls** — `MODEL_HULLS[model_id]`, a BVH over a model's brush AABBs,
  built once per model in `model_set_bvh` (`physics.h`).
- **World BVH** — `ENTITY_BVH`, a BVH over live entity AABBs, rebuilt each frame
  in `build_entity_bvh`. It `bvh_refit`s when the collider count is unchanged
  (cheap, O(N)) and `bvh_build`s when it changed — exactly the refit/rebuild
  tradeoff the library documents. `AABB_IDS[prim_index] → entity_id` bridges BVH
  primitive indices back to entities.

Every spatial question goes through it, always with the **callback + `void*
user`** idiom:

- Collision broadphase: `bvh_self_overlap(&ENTITY_BVH, handle_collision, 0)`,
  whose callback runs a brush-level narrowphase via `bvh_aabb_query` against the
  larger entity's hull (`handle_collision` → `handle_subcollision`).
- Mouse picking: `bvh_ray_query_tight` (`physics.h` `ray_intersection`).
- AI target search: `bvh_closest_query` (`ai.h`).
- Line-of-sight before firing: `bvh_ray_query_tight` (`ai.h ai_ship_shoot`).

**Rule:** no ad-hoc O(n²) entity scans for spatial questions — build/query the
BVH. (One legacy O(n²) loop survives in the unused `ai_ship_select_target`; it is
dead and should not be a template.)

---

## 8. `.map` brushes are the universal content format

TrenchBroom `.map` files are the editor format for **geometry, collision, and
gameplay metadata simultaneously**. `map_loader.h` parses brushes (each a convex
intersection of half-spaces), derives plane equations, computes per-face winding,
and tags interior (hidden) faces of hollow brush assemblies.

One brush set serves three consumers:

- **Render mesh** — `map_brush_triangulate` fills the vertex arena
  (`vlk.h map_to_vertbuffer`). Interior faces are skipped.
- **Collision hull** — narrowphase tests candidate vertices against brush
  *half-spaces directly* (`engine.h handle_subcollision`), re-deriving geometry
  from `face->normal`/`dist` rather than from triangles.
- **Gameplay markers** — entities with `classname`, `origin`, `normal` keys
  (e.g. `info_landingpad`) drive docking (`ai.h`, `engine.h dock_ships`), read
  via `map_entity_get` / `map_entity_get_vec3`.

### Asset pipeline

`scripts/copy-assets.sh` mirrors `assets/` into `bin/assets/`, rewriting `.map`
files from TrenchBroom's **Z-up** to the engine's **Y-up** by swapping Y and Z in
brush plane points, Valve-format UV axes, and vec3-valued entity keys.

**Rule:** new gameplay metadata is new entity keys in the `.map`. If the key is a
vec3, add it to `VEC3_KEYS` in `copy-assets.sh` so the axis swizzle applies.

---

## 9. Renderer: one arena, buffer-device-address, indirect draws

`vlk.h` targets Vulkan 1.4 with dynamic rendering, synchronization2, scalar
block layout, and buffer device address. The shape:

### 8a. One buffer to rule them all

`GAME_VK_ALL_THE_DATA` is a single 256 MB buffer that is **both** host-visible and
device-local, mapped persistently. `vlk_allocateSomeShit(size, out)` is a bump
allocator over it (`ALL_THE_DATA_HEAD`, never reclaimed) returning, for each
sub-range: a host pointer, a device address, and a byte offset.

Everything is a sub-allocation of this one buffer: model vertex buffers, the
model + FX instance buffers, the indirect-draw command buffers, and nuklear's
per-frame vertex/index slabs and font-staging copy.

### 8b. Bindless via BDA

Shaders receive buffer addresses in push constants and pull data through GLSL
`buffer_reference`. Consequences:

- **No vertex input state** — the vertex shader indexes a BDA vertex array by
  `gl_VertexIndex`.
- **Almost no descriptor sets** — the only one in the engine is the UI font
  sampler (`vlk_nk.h`). The 3D path has zero.
- Per-draw data (transform, colour, scale, `dead_time`, `entity_id`) lives in a
  BDA instance array indexed by `gl_InstanceIndex`.

### 8c. Draw submission

`vlk_queueModelDrawCommands` buckets entities by `ENTITY_MODEL` into the instance
buffer each frame and writes one `VkDrawIndirectCommand` per model id; a single
`vkCmdDrawIndirect` issues them all. This is an O(models × entities) CPU pass per
frame — acceptable at current scale, and the natural place to optimize later
(e.g. a GPU culling/compaction pass) if it shows up in a profile.

### 8d. Frame pacing

One command buffer, one fence, immediate present mode. The CPU waits on the fence
**after acquire and again after submit** — i.e. single-buffered, no frames in
flight. This is a deliberate simplicity choice. If smoothness becomes a goal,
frames-in-flight (N command buffers + N fences + acquire/submit semaphores) is
the known next step.

### 8e. Conventions baked into shaders

- Y-flip is done by multiplying clip position by `vec4(1,-1,1,1)` in the vertex
  shaders, not via a negative viewport.
- The model **vertex** shader carries the per-frame "smarts": instancing, per-
  axis scale, the `dead_time` explode-along-normal death animation, and a single
  hardcoded directional light. Lighting and animation live in the vertex stage.

**Rule:** new GPU data is a bump-allocation from the arena, passed by device
address in a push constant. Do not create standalone `VkBuffer`s, do not add
vertex input state, and add a descriptor set only when a sampler leaves no choice.

---

## 10. UI: immediate-mode, retained through global sentinels

`ui.h` drives nuklear entirely from a handful of module globals
(`CONTEXT_MENU_ENTITYID`, `UI_CTRCTS_ENTITYID`, etc.), using `-1` for "closed".
The UI reads the entity / contract / inventory tables directly — same SoA
coupling as the rest of the game, which is correct for this design. Input is
routed to nuklear first; clicks are ignored for world picking when
`nk_window_is_any_hovered` (`main.c`).

---

## 11. Naming & style conventions

- **Global tables**: `SCREAMING_SNAKE_CASE`. (Note: both true constants like
  `MAX_ENTITIES` and mutable tables like `ENTITY_TRANSFORM` use this case; there
  is currently no visual distinction between them.)
- **Functions**: module-prefixed `snake_case` — `vlk_`, `vlk_nk_`, `ai_`,
  `map_`, `bvh_`, `entity_`, `contract_`, `faction_`.
- **Math**: cglm throughout, array-API form (`glm_vec3_*`, `glm_mat4_*`), out-
  param last.
- **Vulkan**: compound-literal designated initializers passed directly as
  arguments are the house style; `VK_WRAP` / `VK_WRAP_ARR` wrap calls and
  enumerate-twice patterns.
- **Formatting**: clang-format LLVM base + `AlignConsecutiveAssignments`. `.clangd`
  forces `-xc` (treat headers as C).

---

## 12. Outstanding issues

Known drift, suspected bugs, and decisions-to-make are **not** tracked in this
document — they live as one file per issue under [`issues/`](issues/), indexed by
[`issues/README.md`](issues/README.md). This keeps the architecture doc about how
things *are meant to work*, and the issues folder about what's *wrong or
undecided*. When an issue is resolved, the fix should make the code match this
doc (or this doc should change).

---

## 13. The canon, in one screen

1. **One TU.** New subsystem → `GAME_*`-guarded module header, included by
   `main.c` in dependency order. Reusable + standalone → stb-style
   `<LIB>_IMPLEMENTATION` library.
2. **Entities are `uint32_t` indices.** State is SoA tables named
   `SUBSYSTEM_FIELD[MAX_ENTITIES]`. No entity struct. Add a component = add a
   table + initialize the new slot (only) in `make_entity` / `entity_copy`.
3. **Default-zero everything you can** and lean on demand-zero `.bss` (§5);
   reserve `-1` sentinels for fields where `0` is valid, and record them in §3b.
4. **Systems are loops over `ENTITY_COUNT`,** registered by being called from
   `sim_loop` in the right phase. `sim_loop` is the schedule.
5. **All spatial queries go through `bvh_*`** with `callback + void* user`. No
   ad-hoc O(n²) scans.
6. **`.map` is the one content format** — geometry, collision, and entity
   markers. New metadata = new entity keys (+ `VEC3_KEYS` for vec3s).
7. **GPU memory = bump-allocate from the arena, pass by BDA.** No new buffers,
   no vertex input state, descriptors only for unavoidable samplers.
8. **Immediate mode everywhere** — rebuild instance buffers, draw commands, and
   UI from the tables each frame; don't retain.
