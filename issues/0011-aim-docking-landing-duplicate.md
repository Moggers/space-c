# `AIM_DOCKING` and `AIM_LANDING` are both `3`

- **Status:** open
- **Severity:** bug
- **Area:** ai

## What
The AI mode constants collide:

```c
#define AIM_IDLE    0
#define AIM_KILL    1
#define AIM_COLLECT 2
#define AIM_DOCKING 3
#define AIM_LANDING 3   // same value as AIM_DOCKING
```

Any code that means to distinguish "approaching to dock" from "final landing"
can't — the two states are indistinguishable. Checks like
`AI_MODE[ship] == AIM_DOCKING` also fire for `AIM_LANDING` and vice-versa.

## Where
- `src/ai.h` — the `AIM_*` defines and their uses

## Fix
Give `AIM_LANDING` a distinct value (`4`), or — if the distinction isn't actually
needed — delete `AIM_LANDING` and use `AIM_DOCKING` everywhere. Decide whether the
two phases are genuinely separate before picking.
