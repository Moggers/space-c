# Frame `time` is `float`, uninitialized on the first frame

- **Status:** open
- **Severity:** bug
- **Area:** engine / main loop

## What
The main loop's timing variable is a `float` and is read before being written on
the first iteration:

```c
float time;                       // uninitialized
while (!done) {
  float delta_t_f32 = ((float)(SDL_GetTicksNS() - time)) / 1e9f;  // reads garbage frame 1
  time = SDL_GetTicksNS();        // u64 nanoseconds stored into a float
  ...
}
```

Two problems:

1. **Uninitialized first read** — `delta_t_f32` on frame 1 is garbage, so the
   first simulation step advances by an arbitrary amount.
2. **`u64` ns into `float`** — `SDL_GetTicksNS()` returns a `Uint64` nanosecond
   count. A `float` has ~24 bits of mantissa, so once the process has been up a
   few seconds the timestamp loses millisecond resolution, and the subtraction
   `now - time` is computed after both have been crushed to `float`.

## Where
- `src/main.c` — frame-timing at the top of the main loop

## Fix
- Store the timestamp as `Uint64` (nanoseconds), initialised from
  `SDL_GetTicksNS()` before the loop.
- Compute the delta in integer ns, then convert the *small* difference to float:
  `float dt = (float)(now - last) / 1e9f;`
- Optionally clamp `dt` to avoid a huge first/hitched step.
