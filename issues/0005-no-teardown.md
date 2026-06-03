# No teardown / no slot recycling

- **Status:** open
- **Severity:** design
- **Area:** engine

## What
Nothing is ever released:

- `bvh_free` / `map_free` exist but are never called.
- The 256 MB GPU arena (`GAME_VK_ALL_THE_DATA`) is allocated once and never
  freed; `vlk_allocateSomeShit` only bumps `ALL_THE_DATA_HEAD`.
- Entity slots never recycle; `ENTITY_COUNT` only grows. Dead entities linger as
  flagged-inert rows.

This is a legitimate "runs until quit" stance and pairs intentionally with the
demand-zero `.bss` strategy (`ARCHITECTURE.md` §5). The issue is that it's
**unstated and unbounded**, not that it's wrong.

## Why it can bite
A long session that spawns many short-lived entities (bullets, asteroid splits)
walks `ENTITY_COUNT` upward forever. Every per-frame system loops `0 ..
ENTITY_COUNT`, so frame cost grows with *total ever-spawned*, not *currently
alive*. The arena bump allocator has the same one-way property for any
per-entity GPU allocation.

## Decision / fix
Decide and document one of:

1. **Accept it** — confirm sessions are short enough that monotonic growth is
   fine, and say so in `ARCHITECTURE.md`.
2. **Recycle slots** — add a free list for entity ids (bullets/splits are the
   obvious churners) so `ENTITY_COUNT` reflects live entities.
3. **Compact** — periodically swap dead rows to the tail and shrink the live
   range.

Tied to issue 0003 (reserving slot 0) if a free list is added.
