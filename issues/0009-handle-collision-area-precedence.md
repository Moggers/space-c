# `handle_collision` AABB "area" precedence

- **Status:** open
- **Severity:** bug
- **Area:** physics

## What
`handle_collision` decides which entity is "bigger" (to test the small hull
against the large BVH) using an AABB volume computed like:

```c
uint32_t area_a =
    (hull_a->max[0] - hull_a->min[0] * ENTITY_SCALE[entity_a][0]) *
    (hull_a->max[1] - hull_a->min[1] * ENTITY_SCALE[entity_a][1]) *
    (hull_a->max[2] - hull_a->min[2] * ENTITY_SCALE[entity_a][2]);
```

Operator precedence makes this `max - (min * scale)` — only `min` is scaled, not
`max`. The intended extent is `(max - min) * scale`. Also note the result is
truncated to `uint32_t`, and a unit-cube hull spans `[-?..?]` so the unscaled
subtraction is meaningless.

Consequence: the bigger/smaller decision (and thus which entity's BVH is queried)
can be wrong, degrading or skewing narrowphase.

## Where
- `src/engine.h` — `handle_collision` (`area_a` / `area_b`)

## Fix
Parenthesise the extent and scale the whole thing:

```c
float area_a =
    ((hull_a->max[0] - hull_a->min[0]) * ENTITY_SCALE[entity_a][0]) *
    ((hull_a->max[1] - hull_a->min[1]) * ENTITY_SCALE[entity_a][1]) *
    ((hull_a->max[2] - hull_a->min[2]) * ENTITY_SCALE[entity_a][2]);
```

Use a float (not `uint32_t`) for the comparison.
