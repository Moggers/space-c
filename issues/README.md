# Issues

Outstanding nasties: bugs, drift, and decisions-to-make. One file per issue so
they can be edited, closed, and referenced independently. This is the counterpart
to [`../ARCHITECTURE.md`](../ARCHITECTURE.md): that doc says how things are *meant*
to work; these files track what's *wrong or undecided*.

**Convention:** one Markdown file per issue, `NNNN-short-slug.md`. Each starts
with a status/severity/area block. Resolving an issue should make the code match
`ARCHITECTURE.md` (or change that doc). When closed, set `Status: closed` and note
the commit, or delete the file — your call, but be consistent.

**Severity:** `bug` (wrong behaviour) · `correctness` (latent / conditional bug)
· `design` (decision to make) · `cleanup` (tidy-up, no behaviour change).

| # | Issue | Severity | Area |
|---|-------|----------|------|
| [0001](0001-two-picking-systems.md) | Two picking systems (CPU raycast vs GPU id-buffer) | design | rendering |
| [0002](0002-faction-0-overloaded.md) | Faction 0 means player *and* world *and* "skip AI" | correctness | ai / engine |
| [0003](0003-entity-0-informally-null.md) | Entity id 0 is only informally "null" | correctness | engine |
| [0004](0004-engine-double-include-and-max-entities.md) | `engine.h` double-included; `MAX_ENTITIES` defined twice | cleanup | build |
| [0005](0005-no-teardown.md) | No teardown / no slot recycling | design | engine |
| [0006](0006-materials-h-empty.md) | `materials.h` is an empty placeholder | cleanup | rendering |
| [0007](0007-map-to-vertbuffer-nested-loop.md) | `map_to_vertbuffer` doubly-nested loop | bug | rendering |
| [0008](0008-contract-add-wrong-index.md) | `contract_add` indexes the wrong table dimension | bug | contracts |
| [0009](0009-handle-collision-area-precedence.md) | `handle_collision` AABB area precedence | bug | physics |
| [0010](0010-ai-ore-handoff-wrong-slot.md) | AI ore hand-off writes the wrong inventory slot | bug | ai |
| [0011](0011-aim-docking-landing-duplicate.md) | `AIM_DOCKING` and `AIM_LANDING` are both `3` | bug | ai |
| [0012](0012-frame-time-float-uninitialized.md) | Frame `time` is `float`, uninitialized first frame | bug | engine |
