# Stage 2 prototype scratch

Not wired into the build (no CMakeLists here yet). Kept as reference/reproduction for the next
session — see `docs/ARM32_JIT.md`'s Stage 2 section for full context, what's proven, and what's
left.

- `arm32asm.h` — the Thumb-2 encoder (movw/movt/mov/push/pop/blx/bx), every encoding verified
  empirically against `arm-linux-gnueabihf-as`/`objdump`.
- `test_emit.cpp` — proves the block calling shape (push/load-args/blx/.../pop) works end to end on
  real Thumb-2 machine code under `qemu-arm`, calling two plain non-inlined functions with baked-in
  operands.
- `test_callwrap.cpp` — proves `DSP::callInstruction()` (dsp.h) correctly reconstructs and calls a
  real dsp56300 handler resolved via `DSP::resolvePermutation()`, matching a direct call through the
  member pointer. Needs the built `libdsp56kEmu.a`/`libdsp56kBase.a`/`libasmjit.a`/`libvtuneSdk.a`
  (build this repo via schwung-monomodule's `-DMNM_DSP56300_DIR=<this repo>` CMake option, or any
  other consumer that builds `source/dsp56kEmu`) to link against.
- `mnm_arm32block.cpp` — the actual Stage 2 block compiler: walks a P-memory range (currently
  Stage 1's sine-table loop body, `$100091-$10009a`), resolves each instruction the same way
  `op_ResolveCache` does, and emits the Thumb-2 call sequence. Belongs in schwung-monomodule's
  `tools/bench/` (needs `MonoVoice`/`Firmware` from that repo, plus `arm32asm.h` alongside it) —
  copy it there rather than trying to build it from this repo alone. See `docs/ARM32_JIT.md`'s
  Stage 2 section for the CMakeLists line to add and the exact build/deploy/run commands used
  (including the device-blocking bug found while testing it, which isn't this file's fault).

To rebuild/run (x86 example; swap `g++` for `arm-linux-gnueabihf-g++` + `qemu-arm` for the ARM
target — see ARM32_JIT.md for the exact commands used):

```
g++ -O2 -std=c++17 -DASMJIT_STATIC \
  -I../../source/dsp56kEmu -I../../source/dsp56kEmu/.. -I../../source/dsp56kBase/.. \
  -I../../source/asmjit/src \
  test_callwrap.cpp <path-to-built-libdsp56kEmu.a etc> -lpthread -o test_callwrap
```
