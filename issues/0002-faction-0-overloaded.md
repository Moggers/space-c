# Faction 0 means player *and* world *and* "skip AI"

- **Status:** open
- **Severity:** correctness
- **Area:** ai / engine

## What
Faction id `0` is overloaded with three incompatible meanings:

1. `faction_create("Player")` is called first, so the **player faction is 0**.
2. World objects (asteroids, `DEBUG_MARKERS`) are **created with faction 0**.
3. `ai_accept_contracts` treats `ENTITY_FACTION[e] == 0` as **"skip — don't run
   AI for this entity."**

It works today only because every faction-0 ship is human-controlled. It breaks
the moment you want an AI-driven player-faction ship: the AI would silently skip
it. (`-1` is separately used for projectiles/neutral in `ENTITY_FACTION`.)

See `ARCHITECTURE.md` §4 (Index spaces) for the broader "reserve 0 = null in
every id space" convention this violates.

## Where
- `src/main.c` — `faction_create("Player")` first; asteroids/markers passed `0`
- `src/ai.h` — `ai_accept_contracts` skips `ENTITY_FACTION[entity_id] == 0`
- `src/faction.h` — `FACTION_COUNT` starts at 0

## Fix
Reserve faction `0` as an explicit `"Neutral"`/world faction:

- In `faction.h`, create faction 0 = `"Neutral"` at startup (or make
  `faction_create` reserve index 0 the way contracts/models reserve theirs).
- Give the player a real faction id ≥ 1.
- Replace the `== 0` AI-skip test with an explicit predicate — e.g. a
  `FACTION_IS_AI[]` flag or "skip the human-controlled entity only" — so
  "neutral" and "don't run AI" stop being the same number.
