# `materials.h` is an empty placeholder

- **Status:** open
- **Severity:** cleanup
- **Area:** rendering

## What
`src/materials.h` is 0 bytes — a placeholder for an intended material/texture
subsystem that doesn't exist yet. Currently all geometry is shaded with a single
hardcoded directional light in `model.vert` and a per-entity colour; brush
texture names (`__TB_empty`, etc.) from `.map` files are parsed but unused.

## Where
- `src/materials.h` (empty)
- `src/shaders/model.vert` (hardcoded lighting), `map_face::texture` (parsed,
  unused)

## Decision / fix
Either delete the empty file until there's something to put in it, or scope what
"materials" should own (texture atlas? per-face material ids from brush texture
names? PBR params?) and capture that as a design note. Don't leave a 0-byte file
implying a subsystem that isn't there.
