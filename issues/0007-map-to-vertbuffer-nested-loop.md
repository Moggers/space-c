# `map_to_vertbuffer` doubly-nested loop over entity_count

- **Status:** open
- **Severity:** bug
- **Area:** rendering

## What
The second (write) pass in `map_to_vertbuffer` has two nested loops over
`m->entity_count`, the inner one shadowing the outer loop variable `t`:

```c
for (uint32_t t = 0; t < m->entity_count; t++) {
  for (uint32_t t = 0; t < m->entity_count; t++) {   // shadows outer t
    for (uint32_t i = 0; i < m->entities[t].brush_count; i++) {
      ...
    }
  }
}
```

The size-counting pass above it loops correctly (single loop over
`entity_count`). So for any map with `entity_count > 1`, the write pass emits
roughly `entity_count ×` too many vertices, overrunning the buffer that was sized
by the counting pass.

It hasn't blown up yet because the loaded models are effectively single-entity
for geometry purposes, but it's a latent buffer overrun.

## Where
- `src/vlk.h` — `map_to_vertbuffer` (the write loop)

## Fix
Delete the redundant inner loop so the write pass mirrors the count pass: one
loop over `entity_count`, one over `brush_count`.
