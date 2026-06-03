# Two picking systems (CPU raycast vs GPU id-buffer)

- **Status:** open
- **Severity:** design
- **Area:** rendering / input

## What
Two implementations of "what did the user click on" coexist; only one runs.

- **CPU raycast (live):** right-click in `main.c` un-projects to a world ray and
  calls `ray_intersection` (`physics.h`), which walks `ENTITY_BVH` then the
  per-model hull BVH.
- **GPU id-buffer (half-built):** `src/shaders/distinct.comp` is compiled to
  `select.spv` by the `Makefile`; `SelectionDataBuffer` / `SelectionPC` are
  declared in `vlk.h`; `ModelInstanceData.entity_id` is written for every
  instance — but nothing ever binds an id image, dispatches the compute shader,
  or reads the result back.

## Where
- `src/main.c` (right-click handler), `src/physics.h` (`ray_intersection`)
- `src/shaders/distinct.comp`, `Makefile` (`select.spv` target)
- `src/vlk.h` (`SelectionDataBuffer`, `SelectionPC`, `entity_id` field)

## Decision / fix
Pick one path and delete the other.

- Keeping CPU raycast: drop `distinct.comp`, the `select.spv` Makefile target,
  the selection structs, and (if nothing else needs it) the `entity_id` field.
- Keeping GPU id-buffer: finish it (render entity ids to an `r32i` image,
  dispatch `distinct.comp`, read back the bitset) and retire `ray_intersection`
  for picking — though note `ray_intersection` is also used by AI navigation
  (`find_correction_location`) and shooting, so it stays regardless.

Given raycast is already needed for AI, "keep CPU raycast" is the low-effort call
unless the id-buffer is wanted for box-select / many-pixel queries.
