# Entity id 0 is only informally "null"

- **Status:** open
- **Severity:** correctness
- **Area:** engine

## What
Several places treat entity id `0` as "none", but nothing enforces that slot 0 is
actually a null entity:

- `ray_intersection` leaves `incident_entity = 0` on a miss; callers test
  `incident_entity > 0` (`main.c` picking, `find_correction_location`).
- `CONTRACT_CLAIMING_ENTITYID[c] != 0` means "claimed", so entity 0 can never
  claim a contract.

This is correct **only by luck of allocation order**: `main.c` creates 10
non-colliding `DEBUG_MARKERS` first (entities 0–9, `ENTITY_COLLIDER_GROUP = 0`),
so no pickable/collidable/claiming entity is ever id 0. Reorder startup and "null"
becomes indistinguishable from "entity 0".

See `ARCHITECTURE.md` §4c.

## Where
- `src/physics.h` — `ray_intersection`, `find_correction_location`
- `src/main.c` — `output.incident_entity > 0`
- `src/ai.h` / `src/engine.h` — `CONTRACT_CLAIMING_ENTITYID != 0` semantics

## Fix
Make slot 0 a permanent, inert "null entity":

- Reserve entity 0 at startup (a dead entity with no model/collider), so real
  entities always have id ≥ 1 — mirroring how model/contract ids reserve 0.
- Then `> 0` / `!= 0` "none" tests become structurally true rather than
  accidentally true.
