# `engine.h` double-included; `MAX_ENTITIES` defined twice

- **Status:** open
- **Severity:** cleanup
- **Area:** build

## What
- `main.c` includes `engine.h` twice (once via the explicit `#include
  "./engine.h"`, once via `#include "engine.h"`). Harmless under the
  `#ifndef GAME_ENGINE` guard, but noise.
- `#define MAX_ENTITIES 2000000` appears in **both** `entities.h` and `engine.h`.
  Identical today, but two sources of truth for the most important sizing
  constant in the codebase.

## Where
- `src/main.c` — duplicate `#include` of `engine.h`
- `src/entities.h` and `src/engine.h` — duplicate `#define MAX_ENTITIES`

## Fix
- Remove the redundant `engine.h` include from `main.c`.
- Keep `MAX_ENTITIES` defined only in `entities.h` (the module that owns the
  tables); delete the copy in `engine.h`.
