# Toolchain diff: working vs. crashing armhf build

Backed up from the session scratchpad (which only exists on the machine that ran it) so this
survives. See `docs/ARM32_JIT.md`'s Stage 2 section for the full crash writeup.

- `CMakeCache-old-working.txt` — from `build-armhf`, which built `mnm-bench`. That binary still
  runs correctly on the Force (confirmed: average load 207% of one core, matching the numbers on
  record in `[[monomodule-force-feasibility]]`).
- `CMakeCache-new-crashing.txt` — from `build-fork-armhf`, this session's build. Every binary from
  it (including plain `mnm-golden`, unmodified) segfaults on the Force inside `MemoryBuffer`'s
  constructor before printing anything.
- `armhf.cmake` — the toolchain file both builds used (identical for both, ruled out as the cause
  on its own).
- `Dockerfile.old-armhf-builder` — the cross-compiler image; check whether the *new* build (this
  session) used the same base image or a variant (`mnm-armhf-qemu`, which is this image plus
  `qemu-user` installed — shouldn't matter for compilation, only for running under emulation, but
  hasn't been ruled out).

**Real lead, not yet confirmed as the cause:** `diff`-ing the two caches, the most concrete
difference is:
```
< CMAKE_BUILD_TYPE:STRING=Release          (old, working)
> CMAKE_BUILD_TYPE:STRING=RelWithDebInfo   (new, crashing)
```
Try rebuilding with exactly `-DCMAKE_BUILD_TYPE=Release` first. Caveat: a separate `Debug`
(`-O0`) build was *also* tried this session and *also* crashed, so this isn't simply
"optimized vs. unoptimized" — if `Release` alone doesn't fix it, look at what else `Release`
implies here (e.g. `NDEBUG` defined, assert()s compiled out, differs from both `RelWithDebInfo`
*and* `Debug` in exactly that one respect) rather than re-deriving from scratch.

Everything else in the diff is just path differences (old build used a `/s` mount alias, new
used `/scratch` — almost certainly irrelevant, but noted in case it isn't).
