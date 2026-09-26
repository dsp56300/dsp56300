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

**Update 2026-09-26 (later session):** the "just path differences" above are not all harmless.
`MNM_DSP56300_DIR` / `ASMJIT_DIR` show the two builds compiled **different dsp56300 source trees**:
old = `/s/dsp56300` (a scratchpad checkout, now gone; most likely the fork at or near `a750f285`,
whose commit message records mnm-bench running on-device), new = `/dsp56300-fork` (this branch's tip).
So the crash may come from a source change after `a750f285`, not the compiler flags. Candidates:
`a0c86d8e` (memory.cpp OOB alias) and the `dsp.h` changes (`1db3f2bb`, `0cfaa42d`, `51d33b8c`).
Bisect: build mnm-golden against `a750f285` using `-DCMAKE_BUILD_TYPE=Release`, then walk forward.
Also note that `a750f285` defines `HAVE_X86_64` on armv7; check any code gated on it.
