# A 32-bit ARM JIT backend

**Status: investigating.** This fork exists because upstream dsp56300's JIT only targets x86-64 and
64-bit ARM (`source/dsp56kEmu/jittypes.h`), and the maintainers consider 32-bit unsupportable at
their scale (Virus-class synths, [gearmulator#44](https://github.com/dsp56300/gearmulator/issues/44):
"a 32 bit version is not possible ... would not run in realtime"). That verdict was about ~100M+
DSP instructions/second (Virus). Monomachine emulation (Monomodule) needs ~21M/s for one track,
which may be a different answer -- this fork is where we find out, without touching upstream.

Driving use case: porting [Monomodule](https://github.com/shnolk/monomodule) (via
[schwung-monomodule](https://github.com/legsmechanical/schwung-monomodule)'s engine glue) to an
Akai Force/MPC VST2 plugin (`sd88me/mpc-vst-monomodule`, uses this repo as a submodule). Any working
32-bit ARM JIT here benefits every dsp56300-based product on 32-bit ARM (Force is Cortex-A17,
armv7); it is not Monomodule-specific.

## Why upstream won't take this

- 32-bit is explicitly unsupported (README, gearmulator#44). Adding a whole JIT backend for it is
  out of scope for them.
- The JIT is built on [asmjit](https://github.com/asmjit/asmjit), which has no AArch32 (32-bit ARM)
  emitter. We can't extend asmjit's existing back end; a 32-bit ARM code emitter has to be written
  from scratch, scoped to only what dsp56300 needs (see Stage 2).

Keep our changes additive where possible (new files, minimal edits to shared ones) so pulling in
upstream fixes stays cheap.

## Baseline (2026-09-26, Monomodule engine, interpreter only, Force = Cortex-A17 armv7 @ 1.8GHz)

Per 128-frame block against the 2902us deadline, `mnm-bench` on-device:

| machine | load |
|---|---|
| GND SIN (lightest) | 149% |
| FM+ PAR | 220% |
| SID 6581 | 250% |
| DPRO DENS (heaviest) | 284% |

All miss the deadline; average 209%. x86 interpreter build of the same code: FM+ at 33% -- the
Force is ~6.6x slower per emulated DSP instruction than a desktop x86 core.

Tried and rejected/kept:
- `-marm` instead of Thumb-2: 6% *slower* -- rejected.
- PGO (profile on-device, rebuild): ~14% faster (FM+ 220% -> 189%). Kept as a cheap win regardless
  of the JIT outcome, but nowhere near enough alone.
- Interpreter profile (callgrind): cost is spread across decode/AGU/memory/ALU, no single hotspot.

## Stage 0 (done, 2026-09-26): is the instruction mix small enough to bother?

Instrumented interpreter build (a local patch, not yet in this repo -- see `sd88me/mpc-vst-monomodule`
scratch history), ran upstream's `mnm-golden` fixed script on all 22 machines, histogrammed executed
(ALU-part, move-part) instruction pairs.

- 62 instruction-pair forms = 95% of executed instructions (109 = 99%), out of 176 seen.
- Decomposed: ~16 arithmetic mnemonics (MAC/MPY/MACR/ADD/ASL/DIV dominate) x ~8 move address forms.
- Multiply-accumulate alone is ~1/3 of all execution -- ARM32 has a native 32x32->64 MAC instruction,
  good fit for the accumulator's low 48 bits at least.
- Hot code is 1.3-1.8k program addresses per machine -- compiles fast, fits in cache.
- Parallel (ALU+move in one instruction) = 59% of execution.
- Side finding: interpreter's golden-script output hash matched the JIT's for DPRO DDRW/DENS/VO-6,
  didn't for the FX machines checked (THRU onward) -- narrows stage 1's work.

**Verdict: proceed.** A JIT only needs to special-case a few dozen forms well; everything else can
fall back to the interpreter without dominating the cost, *if* the covered forms are cheap enough
and the fallback rate stays low.

## Stage 1 (done, 2026-09-26): make the interpreter bit-exact against the JIT

Needed regardless of the JIT outcome -- it's both the correctness fallback path and the reference
every later stage is checked against. All work below runs entirely on x86 (built against this fork
via schwung-monomodule's `-DMNM_DSP56300_DIR`, no device needed); the Force is only needed for later
timing.

- [x] Resolved upstream's known interpreter/JIT divergence on out-of-range memory reads
      ([dsp56300#8](https://github.com/dsp56300/dsp56300/issues/8)). Root cause confirmed empirically:
      `Memory::get()`/`dspWrite()` unconditionally bailed out (return 0 / drop the write) for any
      offset >= the area's nominal size, *even when the MMU-backed `MemoryBuffer` is active* -- but
      the MMU buffer already maps the whole $000000-$ffffff DSP address range, aliasing every
      out-of-range address onto one shared scratch region (`memorybuffer.cpp`'s comment describes
      this). The JIT (`jitmem.cpp`) already reads/writes through that aliased scratch memory with no
      bounds check at all when MMU support is present; the interpreter didn't. Fixed in `memory.cpp`:
      skip the bounds bail-out when `hasMmuSupport()`. Verified: before the fix, `mnm-golden` (x86,
      `MNM_DSP_INTERP=1`) spammed thousands of `LOG_ERR_MEM_READ` lines across the script; after, zero.
      In practice the scratch region reads back as 0 either way in our engine so this alone didn't
      change any golden hash, but it's a real, confirmed divergence source worth having fixed (matters
      more once code starts writing through it) and is upstreamable as-is.
- [ ] **Still open, still the actual cause of the 10/22 mismatches**: `mnm-golden` hash-compared JIT
      vs. interpreter (x86, this fork, after the fix above) -- still exactly the same 10 machines
      mismatch as in stage 0's spot check: FM+ STAT/PAR/DYN, GND SIN, SWAVE SAW/PULS, DPRO WAVE,
      REVERB, RINGMOD, PHASER. All 12 others (GND ---/NOIS, SWAVE ENS, SID 6581, DPRO BBOX/DDRW/DENS,
      VO-6, THRU, CHORUS, DYNAMIX, FLANGER) already match.
      Narrowed with a raw-sample diff (`mnm-golden`'s `raw-out-dir` arg) on GND SIN: JIT and
      interpreter audio first diverge at **sample 24 of 264448** (i.e. within the first millisecond,
      not after some later parameter sweep), and by a small amount (0.128 vs 0.131) -- not a gross
      garbage/clipped value. That's more consistent with a rounding/precision difference in early
      oscillator or sine-table-build arithmetic than a dramatic saturation blowup, so the "accumulator
      overflow corrupts the sine table" bug from Schwung's `DspEngine.cpp` comment may be real but is
      probably not (solely) this. `getA<T>()`/`getB<T>()` (`limit_transfer`) and `alu_add`/`alu_sub`'s
      `limit_arithmeticSaturation` calls were checked and look correct on inspection -- next step is a
      per-instruction trace comparison (interpreter vs. JIT, same script, diff first mismatching PC)
      rather than more code reading; this is where stage 1 picks back up.
- [ ] Once traced and fixed, `mnm-golden` must hash-match on all 22 machines.

**Second session's findings (2026-09-26, continued):** narrowed the sine-table bug to an exact,
reproducible test vector, and ruled out two plausible-looking causes by testing them, not just
reading code -- both changes below are harmless/more-correct but confirmed **not** the cause:
- Dumped the built 8192-entry sine table (Y:$14A000) directly (no audio rendering needed -- it's
  built once during engine init) and diffed interpreter vs. JIT. They agree exactly through index
  0x1800 (both = 0x800000, i.e. exactly -1.0, the table's minimum). From index **0x1801 onward, every
  remaining entry differs** (2047 of 8192): JIT continues correctly (0x800002, 0x80000a, 0x800016...
  -- values easing up from -1.0), the interpreter's values flip sign (0x7fffff, 0x7fffec, 0x7fffda...
  -- near +1.0 instead). This is a clean, cheap repro: `mnm-dumptable <os.syx>` (a small new tool,
  not yet committed anywhere durable -- see below) vs. the same with `MNM_DSP_INTERP=1`.
- Ruled out: `limit_arithmeticSaturation`'s bit-check (bits 55/48/47 only, vs. the JIT's full
  sign-extend-and-range-check in `alu_saturateSM`) -- genuinely inconsistent logic between the two,
  fixed in `dsp.h`, but `SR_SM` is never set during this script, so the fix is inert here (kept
  anyway, real bug relative to the JIT's own logic, just not *this* bug).
- Ruled out: `alu_mac` (used by `macsu`, one of this loop's instructions) added the old accumulator's
  raw `.var` instead of its sign-extended value, unlike its siblings (`alu_mpy`, `alu_mpysuuu`,
  `alu_dmac`) which all sign-extend first. Looked like a strong match for a sign-flip bug. Turned out
  to be a **numerical no-op**: since the result is masked back to 56 bits (mod 2^56) either way, adding
  the raw two's-complement bit pattern vs. its sign-extended form gives bit-identical results after
  masking -- reverted, not worth the confusion of keeping a change that provably does nothing.
- Not yet checked: the multiply/accumulate chain's actual arithmetic (this loop is a CORDIC-style
  rotation: `mpyuu` (replace, unsigned*unsigned) -> `dmac su` (double-precision MAC, shifts the old
  accumulator right 24 before adding -- a genuinely different accumulation model worth scrutinizing
  on its own) -> `macsu` (normal MAC) -> `dmac ss` -> `asl a` (parallel with `l:(r1),y`) -> `sub y,a`),
  and specifically how `y` (the 48-bit data register, loaded via the parallel move) gets sign-extended
  to 56 bits for the `sub` (`XYto56`/`signextend48to56` -- read but not yet verified against the JIT's
  equivalent bit-for-bit). `alu_dmac`'s own `>>24` old-value handling is also unverified against the
  JIT's `op_Dmac` line by line, beyond confirming both skip SM saturation identically.
- Tooling built for this (in the schwung-monomodule scratchpad, `libs`-linked against this fork via
  `-DMNM_DSP56300_DIR`, not yet copied anywhere durable): `mnm-dumptable` (dumps the sine table for
  diffing), a `MNM_STEP_TRACE` env-gated per-`exec()` register hash hook added to `DspEngine.cpp`
  (useful for coarse checkpointing by matching `getInstructionCounter()` values between runs, but
  **not reliable inside a JIT-compiled hardware loop**: `maxInstructionsPerBlock` does not apply to
  `isRep`/`isFastInterrupt` blocks (`jitblock.cpp:220`), so a `do`/`rep` loop still executes
  atomically -- confirmed by watching `m_instructions` jump by ~8190*10 in one `exec()` call even with
  the block-size limit set to 1). `Jit::setConfig()`/`maxDoIterations` looked like the fix for that
  (meant to give a JIT block an exit point every N loop iterations) but a first attempt to use it
  (setting `maxDoIterations=1` before `resetKeepCode()`, to re-run the init under single-step config)
  hung or never reached the target address within a 20M-step guard -- not debugged further this
  session; if picked back up, check whether `resetKeepCode()` after `destroyAllBlocks()` breaks the
  init handshake, and whether `Y:$14A000`'s `r0` register actually holds the bridged absolute address
  ($14A000+index) rather than a bare index (this was left unverified and may just be a wrong target
  address in the test, not a real hang).
**Resolved.** Found it by building the from-scratch reproducer this doc's previous revision
recommended: captured the exact register state at the real divergence boundary (via a temporary
debug hook in `do_exec`'s loop, dumping registers once `r0` reached the target table address), then
replayed the loop body's 10 instructions from that exact state, once, in both engines, diffing full
registers after every instruction instead of a hash. The two diverged on the *very first*
instruction (`move l:(r1)+,y`), which pointed straight at the *previous* iteration's `move a,l:(r1)`
(the loop's only other write to that address) rather than anything in the instructions between them.

Root cause: `decode_LLL_read`'s case 4/5 ("A"/"B", used by plain `move a,l:(rN)` /
`move b,l:(rN)`) in the interpreter did a raw `reg.a.var >> 24` / `& 0xffffff` bit split with no
scaling or saturation. The JIT's equivalent (`jitops_decode.cpp`, same case 4/5) runs the full
accumulator through `transferSaturation48` first -- scale(), sign-extend, clamp to the 48-bit signed
range, mask. Those only agree when the accumulator's extension byte is still consistent with bit 47;
any iterative accumulation (this loop's CORDIC-style multiply-accumulate chain) can produce a
borderline value where they aren't, and once that happens, the two engines write *different* raw
patterns into the loop's read-back buffer even though the buffer's *visible* effect stayed masked for
a few more iterations by an unrelated coincidence: the table-write instruction (`move a,y:(r0)+`)
goes through `limit_transfer` (24-bit saturation), which happened to saturate both engines' already-
different values to the identical `$800000` right at the table's minimum, so the divergence was
invisible in the sine table itself until the values moved away from that boundary. Fixed by adding
`DSP::limit_transfer48()` (mirrors `transferSaturation48` bit-for-bit) and using it in
`decode_LLL_read`'s case 4/5, in `dsp.h`/`dsp_decode.inl`.

Verified: `mnm-golden` now hash-matches the JIT on **all 22 machines**, and the sine table dumps
(`mnm-dumptable`, both engines) are byte-identical across all 8192 entries. Stage 1's gate is
cleared -- the interpreter is bit-exact against the JIT for Monomodule's actual firmware, not just
in isolated unit tests.

Debug tools built along the way (schwung-monomodule scratchpad, not committed anywhere durable --
cheap to rebuild from this description if needed again): `mnm-dumptable` (dumps the sine table for
diffing, no audio pipeline needed), `mnm-bodydiff` (replays a captured register snapshot through a
fixed instruction range in both engines, diffing full registers after every instruction -- this is
what actually found the bug, and is the technique to reach for first next time, not instruction-count
tracing, which doesn't survive JIT/interpreter hardware-loop batching, see the ruled-out attempts
above). The `MNM_STEP_TRACE` hook added to `DspEngine.cpp` and the temporary `MNM_DEBUG_R0_HEX` hook
added to `dsp.cpp`'s `do_exec` (used to capture the exact register snapshot fed into `mnm-bodydiff`)
were both removed again after use; not committed.

Going forward, re-run this check (`mnm-golden`, x86, no device needed) after any dsp56300 patch
change -- it's the standing regression gate for the rest of this investigation.

## Stage 2 (in progress, 2026-09-26): kill dispatch overhead only (no real code generation yet)

**Where this stands, for the next session:**

- Chose the calling shape: a compiled block is hand-written Thumb-2 bytes in an mmap'd
  `PROT_EXEC` page, structured as `push {r4,lr}; [load r0=this, r1=op, r2=handlerAddr; blx r2]* ;
  pop {r4,pc}`. Every encoding this needs (`movw`/`movt` 32-bit-immediate-load pair, hi-register
  `mov`, `push {r4,lr}`/`pop {r4,pc}`, `blx`) was derived **empirically** -- assembled a probe with
  `arm-linux-gnueabihf-as`, read the bytes back with `objdump -d`, fit a bit-field formula to
  several different immediates to confirm it, never taken from memory of the ARM ARM. See
  `arm32asm.h` (scratchpad, not yet committed to this repo -- small, worth moving into
  `source/dsp56kEmu/` next session as e.g. `arm32blockemitter.h`).
- Proved the core ABI assumption end-to-end, twice:
  1. On a toy class (x86): a non-virtual member function pointer with no multiple inheritance is
     `{address, 0}` under the Itanium C++ ABI (verified: `sizeof` is 2 pointers, second word is 0,
     and calling the first word as a plain `void(*)(T*, Args...)` with `this` as arg0 works).
  2. On the real thing: `test_emit.cpp` (scratchpad) generates actual Thumb-2 machine code with the
     block shape above and runs it under `qemu-arm` calling two real (non-inlined) C functions with
     baked-in operands -- output matched calling them directly. Then `test_callwrap.cpp`
     (scratchpad) did the same against the *real* dsp56300 handlers: resolved `DSP::op_Inc` via
     `DSP::resolvePermutation()`, confirmed its `TInstructionFunc`'s raw bytes are `{address, 0}`,
     and confirmed `DSP::callInstruction(rawAddress, op)` (the new wrapper, see the dsp.h commit)
     produces identical state to calling the resolved handler directly -- checked on x86 and on a
     real armhf binary under qemu-arm (cross-built via `xbuild/armhf.cmake` against
     `-DMNM_DSP56300_DIR=<this fork>`).
  3. Also reconfirmed, on that same armhf cross-build: `mnm-golden` still hash-matches on all 22
     machines under qemu-arm -- Stage 1's fix holds on a real ARM binary, not just x86.
- Added `DSP::callInstruction(void* rawFunc, TWord op)` and
  `DSP::callParallel(void* rawMove, void* rawAlu, TWord op)` (public, dsp.h) as the bridge: a
  generated block can't easily build a real `TInstructionFunc`
  value (or its address, for `exec_parallel`'s by-const-ref params) without emitting data
  alongside the code, so these wrappers rebuild it from a plain address in ordinary C++ instead.
  Committed and pushed.
- Deliberately **not yet done, and not started**: the actual block compiler (walk P memory from a
  PC using `Opcodes`/`getInstructionTypes` -- same APIs Stage 0's histogram tool already used --
  to find non-parallel vs. parallel instructions and where a basic block ends, mirroring
  `op_ResolveCache`'s *decode* logic in `dsp_ops.inl:547` without its *execution* side effect);
  wiring compiled blocks into `DSP::m_jitEntries`/`execJit()` (or, more likely for a first
  measurement -- see below -- bypassing that entirely); and the actual on-device timing comparison
  against the interpreter, which is the whole point of this stage.
- **Scope decision for finishing this stage, so as not to over-build**: don't try to retrofit this
  into `DSP`'s real lazy `m_jitEntries` dispatch machinery yet (that's tightly coupled to the old
  asmjit-based `Jit` class's own lazy-compile-on-first-call trampoline, which would need real
  surgery to share cleanly). Instead, for *this* measurement: eagerly compile a fixed, known
  region -- Stage 1's already-fully-reverse-engineered sine-table loop body (P:$100091-$10009a,
  10 instructions, see Stage 1's section above for the disassembly) is a good first target, since
  its instructions, operands and correctness are already fully understood -- into one block ahead
  of time, and time N repetitions of calling that block directly vs. N repetitions of
  `dsp.execInterpreter()` over the same 10 instructions, on the Force (`root@192.168.1.44`,
  reachable this session). That answers Stage 2's actual question (is dispatch/decode where the
  time goes) without needing the lazy-compilation/chaining machinery Stage 3+ would need anyway.
- Device confirmed reachable this session (`ssh root@192.168.1.44` -- armv7l).
- Built the compiler: `mnm-arm32block.cpp` (schwung-monomodule scratchpad, not yet committed
  anywhere durable -- move it into this repo's `tools/arm32jit_prototype/` next session) walks
  P:$100091-$10009a (Stage 1's sine-table loop body), uses `Opcodes::getInstructionTypes` +
  `DSP::resolvePermutation` to resolve each instruction exactly like `op_ResolveCache` does (minus
  the execution side effect), and emits the Thumb-2 call sequence via `arm32asm.h`, using
  `DSP::prepareOp`/`callInstruction`/`callParallel`. Confirmed on x86 (decode-only path, since
  Thumb-2 bytes aren't valid x86) that it resolves and decodes the whole loop correctly (mpyuu,
  dmac x2, macsu, the `asl a`/`l:(r1),y` parallel pair, the "sub y,a" pair, the nop, both L-moves).
- **Blocked -- found a real, pre-existing, unrelated bug**: running *any* armhf binary built
  against this fork on the real Force crashes with `SIGSEGV, si_code=SEGV_MAPERR`, PC == fault
  address (`0xf505e7c2` in one run) -- a wild jump, not a bad data access. This is **not** Stage 1
  or Stage 2 code: it reproduces with plain `mnm-golden` (Stage 1's own bit-exactness tool,
  unmodified since it passed), at `-O0`/Debug with no LTO, so it's not an optimizer miscompile
  either. `strace -i` pins it inside `MemoryBuffer`'s constructor (`memorybuffer.cpp`, upstream
  code, not touched by this fork's patches) -- crashes right after the last of a long run of
  `mmap2`+`mlock` pairs (the "map scratch blocks to cover the full $000000-$ffffff range" loop,
  the same code Stage 1 traced through to find the OOB-read alias behaviour). Confirmed this is
  **not** a qemu-user artifact: reproduces identically running the binary natively on the device
  over SSH (qemu-arm was actually flakier/inconsistent in this session for unrelated environment
  reasons -- a loader-path issue that came and went; don't trust qemu-user for this repo without
  `QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf`, and even then treat it as a second check, not ground
  truth -- the device is ground truth).
  - This is confusing against `[[monomodule-force-feasibility]]`'s own numbers, which record real
    interpreter benchmarks (150-285% load) *from the Force*, implying the MMU-backed memory setup
    worked at some point on this exact hardware. Whatever toolchain built *that* binary is not the
    one this session's `mnm-armhf-qemu` Docker image + `xbuild/armhf.cmake` produces (GCC 12,
    `-mcpu=cortex-a17 -mfpu=neon-vfpv4 -mfloat-abi=hard`) -- worth finding that original build
    (or its flags/compiler version) before assuming this is a genuine 32-bit correctness bug in
    `memorybuffer.cpp` rather than a toolchain regression. PC==faultAddr (a wild branch) is more
    consistent with stack/return-address corruption than a straightforward pointer-arithmetic bug,
    which nudges toward "toolchain/ABI mismatch" over "logic bug", but that's not confirmed.
  - Next step if resumed: either (a) track down the toolchain that produced the numbers already in
    `[[monomodule-force-feasibility]]` and rebuild with that instead, or (b) get a real backtrace
    (no `gdb` on-device or in this session's containers; `strace -i` gave the faulting PC but not a
    call stack -- installing `gdb` cross tools, or copying the core file off the device to analyse
    with `arm-linux-gnueabihf-gdb` on the host, is the way to actually see the corrupted call chain).
- Bail-out gate (unchanged, not yet reached): if the compiled block, once actually run, isn't at
  least ~1.3x faster than the interpreter for that loop on-device, stop -- but this can't be
  measured until the above is resolved.

- Decode each DSP instruction once per basic block instead of every execution; emit a straight-line
  chain of calls into the *existing* interpreter opcode handlers with operands baked in, using a
  hand-written ARM32 code emitter (~a dozen instruction encodings: `bl`, load-immediate, stack setup).
- This isolates "is dispatch/decode the cost, or is it the arithmetic itself" before writing any
  DSP-specific native code.
- **Bail-out gate:** if this isn't at least ~1.3x faster than the interpreter on-device, dispatch
  isn't where the time goes and a full native-codegen JIT won't pay for itself either -- stop here.

## Stage 3: native code for the hot forms, in stage-0's histogram order

- MAC/MPY/MACR/ADD/ASL/DIV first (covers the bulk per stage 0), then address-register arithmetic
  (modulo addressing included).
- After each batch: `mnm-golden` must still hash-match, then re-bench on the Force. Track load vs.
  forms-converted to see the curve early.
- **Bail-out gate:** if the trend after ~5 forms can't plausibly reach ~50-60% of a core for the
  heaviest machine, stop -- 32-bit ARM's register pressure (56-bit accumulators, ~12 usable GPRs vs.
  aarch64's JIT having far more to work with) may make this a losing architecture regardless of how
  much code gets converted.

## Stage 4: finish

- Loops (`DO`/`REP`), block chaining, lazy condition-code flags, interrupts.
- Then the VST wrapper/host-test/skin/bench pipeline in `sd88me/mpc-vst-monomodule`.

## Known risk going in

32-bit ARM has ~12 usable general-purpose registers against 56-bit-wide accumulators and lots of
DSP state (X/Y accumulators, address registers, loop registers, condition codes). The existing
aarch64 JIT has roughly double the registers to work with. Expect heavier register spilling than the
64-bit backends; stages 2-3 will show whether that's fatal or just a smaller win than aarch64's.
Rough pre-measurement guess: 25-40 cycles/instruction on the Force's A17 (vs. ~10 on aarch64 Move
hardware, per schwung-monomodule's docs/PERF.md), landing Monomodule around 35-65% of one core if
stage-0's 95% coverage holds and the uncovered 5% doesn't dominate via interpreter fallback.

**Update (end of session):** the original `build-armhf/mnm-bench` binary, the one behind the
recorded Force numbers, *still runs* on the device today and gets past MemoryBuffer init. So the
crash comes from this session's build (`mnm-armhf-qemu` image + `build-fork-armhf`), not from the
hardware or from upstream `memorybuffer.cpp`. Next session: diff the two builds' compile/link
flags (`build-armhf/CMakeCache.txt` vs `build-fork-armhf/CMakeCache.txt`, and the image each was
built with) and rebuild Stage 2's tools the old way.

## Stage 2 result (2026-09-26, later session): gate FAILED -- JIT effort stopped

**The "MemoryBuffer crash" was never a toolchain or memory bug.** schwung-monomodule's `DspEngine`
defaults to the JIT path unless `MNM_DSP_INTERP=1` is set; on armv7 there is no JIT, so `exec()`
loads through a NULL JIT table (gdbserver backtrace: `DspEngine::runUntilTx`, `ldr r2,[r2=0,...]`).
The old `mnm-bench` numbers were taken with that variable set. With it, every build works, both
`a750f285` and the branch tip, built with `Release` in the old `debian:bookworm` +
`crossbuild-essential-armhf` image and `tools/arm32jit_prototype/toolchain-diff/armhf.cmake`.
(The core goes to the vendor's `az01-coredump` handler, so use `gdbserver` from the bookworm
`gdbserver:armhf` deb plus `gdb-multiarch` in Docker to debug on-device.)

**New open issue (not Stage 2): the armhf interpreter isn't bit-exact with x86.** On the branch tip,
the x86 interpreter matches the x86 JIT on all 22 machines (Stage 1 holds). The armhf interpreter,
on the Force *and* identically under qemu-arm (so it's deterministic, not the hardware), matches on the
15 synth machines but differs on all 7 effect machines (THRU, REVERB, CHORUS, DYNAMIX, RINGMOD, PHASER,
FLANGER). The earlier "armhf matches on all 22" note above is wrong. It is likely a 32-bit
portability bug (`long`/`size_t` width or a shift) in the interpreter or glue. It would matter for
shipping the interpreter on the Force.

**Block compiler on-device** (`tools/arm32jit_prototype/mnm_arm32block.cpp`, fixed this session:
it used callee-saved r5 as scratch without saving it (now r12), and it was missing the i-cache
flush before executing the buffer):
- Runs, but **register state differs** from the interpreter: after the 10-instruction sine loop body,
  `a0` and `y0` come out swapped (interpreter a0=$c62f03 y0=$ba9a81, block a0=$ba9a81 y0=$c62f03).
  This looks like the parallel move/ALU ordering in `callParallel` (the move must read its source
  before the ALU writes). Not fixed, since the gate below failed first.
- **Timing, Force, 2M iterations x 10 instr, 3 runs: interpreter 60-62 ns/instr, block 53-54
  ns/instr, speedup 1.12-1.17x. The gate is >= 1.3x, so it FAILS.** The block also skips the
  interpreter's per-instruction interrupt and loop bookkeeping, so a complete version would be slower
  still. Removing dispatch and decode saves only ~12%, so the time is in the opcode handlers
  themselves (the arithmetic and state access), which is what Stage 3 would have to rewrite natively
  anyway. Per this plan's bail-out rule, **the arm32 JIT effort stops here.**

## Interpreter profile on the Force (2026-09-26): flat, no cheap win. Port shelved.

`perf record -F 2000` of `mnm-bench os.syx 3 3` (all 22 machines, 3 s each, pinned to cpu 3,
`MNM_DSP_INTERP=1`, Release, branch tip). The average load was 241.9% of one core. perf came from
Debian bookworm's `linux-perf:armhf` and its dependencies, unpacked into /tmp and run with
`LD_LIBRARY_PATH`; the Force has an `armv7_cortex_a12` PMU but ships no perf.

| share | where |
|---|---|
| 10.5% | `DSP::op_Parallel` (parallel move/ALU dispatch) |
| 8.7% | `DSP::do_exec` (per-instruction fetch/dispatch) |
| 7.9% | `DSP::alu_mpy` |
| 6.6% | `dspExecPeripherals<Peripherals56303>` (run every instruction) |
| 6.3% | `DSP::op_Mac_S1S2` |
| 4.9% | `DspEngine::runUntilTx` (glue: polls HI08 tx and the instruction budget every instruction) |
| ~3% each or less | AGU update, ddddd decode, the Movex/Movey/Movel/Movexy handlers, Asl, Mpy, Add, `Memory::get`, ... (a long tail) |

Reading it:
- There's no dominant helper. The biggest single symbol is 10.5%, and the MAC/MPY arithmetic totals
  about 18%. Hand-optimizing the arithmetic, even perfectly, is worth at most about 1.2x.
- Per-instruction overhead (do_exec + op_Parallel + peripherals + runUntilTx) is about 31%. Batching
  the peripheral tick and the `runUntilTx` poll every N instructions is cheap (it's in the glue),
  but removing *all* of that overhead would cap out around 1.45x, matching Stage 2's 1.13x from
  removing dispatch alone.
- The port needs about 2.5x just to reach 100% of one core, and about 4x for a comfortable 60%. The
  interpreter can't get there. Only a full native-code JIT could, and Stage 2's gate already ruled
  that out as a bet.

**Decision: Monomodule on the Force is shelved.** The fork stays as a record. If revisited, the only
realistic path is a full native JIT for the hot handlers (Stage 3 as written), with the
register-pressure risk noted above.

## Static recompilation gate test (2026-09-26): 1.84-2.06x, registers match

Stage 2 only removed dispatch; the profile showed the cost is inside the handlers (runtime operand
decode, generic flag updates). So this test translates the same 10-instruction loop **ahead of time
into C++**: each instruction becomes a direct call to the interpreter's own handler with a *constant*
opcode, compiled into `dsp.cpp`'s translation unit (`dsp56k_recomp.inl`, via `__has_include`) so GCC
inlines the handlers and folds their decoding away. The handler for each PC is read from the
interpreter's own opcode cache after one interpreted pass (`DSP::getRecompInfo`), so resolution
matches the interpreter by construction. Tools: `tools/arm32jit_prototype/recomp/`.

- **Correctness: PASS.** Identical registers to the interpreter.
- **Stage 2's "a0/y0 swap" was a harness bug, not a compiler bug.** The loop writes `l:(r1)` and
  `y:(r0)+`, and the old harness reset registers but not memory, so the second run read the first
  run's writes. Once memory is restored too, it passes. Stage 2's block was probably correct as well.
  Separately, Stage 2 wrongly treated `$100097` (0x200034, ALU-only) as a parallel pair; the
  interpreter's cache doesn't.
- **Force timing (3 runs, 2M iterations): interpreter 62-66 ns/instr, recompiled 30-36 ns/instr,
  1.84-2.06x.** That passes the 1.3x gate Stage 2 failed, with no hand tuning at all.
- It is **not yet enough on its own.** At ~2x, the 242% average load would drop to ~120% of one core,
  still over budget. Remaining out-of-line calls in the generated code are memory access
  (`Memory::get`, `memWrite`, `decode_MMMRRR_read`) and `alu_asl`. DSP registers also still live in
  the `DSP` object rather than in locals across the block.
- **Next gate:** inline the memory fast path (MMU-backed direct array access) and see whether this
  loop reaches **>= 3x**. If it does, a whole-program recompiler (per-block, hash-checked, falling
  back to the interpreter, plus batching the peripheral tick and `runUntilTx` poll that cost ~11% in
  the profile) is a realistic weeks-scale project. If it stalls around 2x, stop.

### Second gate (same day): 3.73-3.83x with `flatten`, gate passed

Two changes, measured separately. Force, pinned with `taskset -c 3`, 5 alternating runs x 3M
iterations; the performance governor was already set:

| variant | interpreter | recompiled | speedup |
|---|---|---|---|
| inline memory fast path only | 70 ns/instr | 35-36 ns/instr | 1.94-2.04x |
| + `__attribute__((flatten))` on the generated block | 66 ns/instr | 17.3-17.8 ns/instr | **3.73-3.83x** |

Registers matched the interpreter on every run.

- **Inline memory fast path** (`memory.h`): with a valid MMU buffer, `Memory::get`/`dspWrite` are
  inline one-line array accesses; everything else goes to the renamed `getSlow`/`dspWriteSlow`.
  Behaviour is unchanged, and it helps the interpreter too. On its own it made no measurable
  difference to the recompiled block.
- **`flatten`** (emitted by `recomp_gen.py`) makes GCC inline everything reachable from the block:
  `decode_MMMRRR_read`, `DSP::memWrite`, `alu_asl`, plus the now-inline memory access. That's where
  the win is.
- Earlier, unpinned runs swung 60-98 ns/instr for the interpreter alone (MPC load on shared cores),
  so always pin with `taskset` for these measurements.

At ~3.8x, the 242% average interpreter load would come to ~64% of one core. That's close to the
~60% target, before the other planned savings: batching the peripheral tick and the `runUntilTx`
poll (~11% of the profile), and dropping per-instruction bookkeeping the block still does. Caveat:
this is one MAC-heavy loop, and a whole program adds block entry/exit, branches, hardware loops and
interpreter fallback.

**Next: whole-program recompiler.** Walk the DSP program into basic blocks from real executions
(same trick: the interpreter's opcode cache gives the handlers), generate one `flatten` function per
block, dispatch by PC through a table, verify each block's P-memory words at runtime (fall back to the
interpreter on mismatch or unknown PC), and handle DO/REP loops and branches at block ends. Gate for
that stage: `mnm-golden` hash-matches the interpreter on all 22 machines, and `mnm-bench` averages
<= 100% of one core on the Force.

## Whole-program static recompiler: first working version (2026-09-26)

**Result: correct on all 22 machines, Force load 225.5% -> 113.4% of one core (1.99x).** The gate is
<= 100%, so it isn't there yet.

How it works (`tools/arm32jit_prototype/recomp/`, dsp.h/dsp.cpp `recomp*`):
1. `mnm-recomp-discover` (a build with `-DDSP56K_RECOMP_DISCOVERY`, run with `MNM_DSP_INTERP=1`) runs
   mnm-golden's exact workload with a pre-execution hook in `execInterpreter`. It records each executed
   instruction (words, the handlers the interpreter's opcode cache resolved, block-ending kind, whether
   the parallel move needs the ALU/move latch, whether it reads PC), plus entry points and DO loop ends.
   Result: 9043 distinct instructions, no address ever holds two different instructions.
2. `recomp_gen2.py` builds basic blocks (1136 blocks, avg 7.6 instructions, covering 98.4% of executed
   instructions) and emits one `__attribute__((flatten))` `DSP::recompBlock<PC>` per block, plus
   `DSP::recompProgram()` (the block table, a PC index and a covered-address bitmap, all shared by every
   DSP instance).
3. A build with `-DDSP56K_RECOMP -I<dir of dsp56k_recomp.inl>`: `execInterpreter()` runs the block at the
   current PC instead of one instruction, when:
   - the block's P words match (verified lazily, and again after a P write inside it);
   - not in fast-interrupt mode;
   - no active DO loop ends strictly inside the block.

   Otherwise it interprets. DO/REP/WAIT/IFcc always go through the interpreter, and so do DO loops
   (do_exec calls execInterpreter recursively, so loop bodies dispatch to blocks). Interrupts and
   peripherals are checked once per block, and the instruction counter is advanced once per block,
   like the JIT.
4. The generated `.inl` contains firmware opcode words: never commit it (there's a `.gitignore`). Build
   it from your own OS `.syx`.

What's in a block: per instruction, direct calls to the interpreter's handlers with constant opcodes.
There's no PC or opcode-length bookkeeping except where a handler reads it (control flow, LRA, STOP),
and the final PC is set once at block end. Parallel instructions skip the latch unless the move
touches an accumulator the ALU writes (36.7% still need it).

Measured on the Force (SID machine, pinned): ARM instructions 10.1G -> 4.9G, IPC unchanged (~1.1),
L1 I-cache misses 21.7M -> 44.6M (the generated code is several MB), branch misses 110M -> 20M.
Profile: 76% in blocks, 6.9% do_exec, 2.7% runUntilTx, 2.6% peripherals, ~2% leftover interpreter.
Neither per-block instruction counting nor a P-write coverage bitmap made a measurable difference.

**Bit-exactness, and an unexplained difference:** recompiled builds match the x86 interpreter/JIT
hashes on all 22 machines, both on x86 and **on the Force**. The plain armhf interpreter still differs
on the 7 effect machines (see above). It isn't handler resolution: armhf discovery (under qemu)
resolves identical handlers at every PC to x86's. So it's something in the interpreter's
per-instruction path that the recompiled path bypasses; still open. Practically, the recompiled Force
build now produces the same audio as the x86/aarch64 builds.

Next candidates (bigger): specialise address-register updates on the M register values seen at each
instruction (with a runtime guard); run single-block DO loop bodies in a tight loop inside generated
code instead of via do_exec + execInterpreter per iteration; recompile REP'd instructions.

## Stage 3 progress (2026-09-26/27, overnight)

Every step below is bit-exact on all 22 machines (x86 and Force). Force median of 3 pinned runs
(`tools/arm32jit_prototype/recomp/bench.py`, noise about ±1% when MPC is quiet, ±5% otherwise):

| step | commit | avg load |
|---|---|---|
| whole-program recompiler | d6bc6250 | 113% (98-102% on a quieter device) |
| `alu_mpy`: 32x32->64 multiply (one `smull`) | f944993d | ~95% |
| dead-CCR elimination for MPY/MPYR/MAC/MACR (36% of executed instructions) | 3fdc3398 | 89.2% |
| DO loop's final body block run straight from do_exec (like the JIT's in-block loop) | 59a12d1c | 87.9% |
| register->memory parallel moves run before the ALU, no A/B latch | 8cc3a9dd | 85.9% |
| dead-CCR variants for ADD/SUB S,D and ASL/ASR #ii | (gen) | 85.8% |
| whole-DO-loop functions for single-block loop bodies (`recompLoop<PC>`) | 5dcedc92 | 78.7% |
| `limit_transfer`: one unsigned range check | 07076d6d | 73.2% |
| AGU linear-addressing fast path; peripheral branches unlikely | 269d6867 | 67.6% |
| 56-bit `signextend` via high word only (one `sbfx`); `scale()` single test | e1223e6f | 60.5% |
| HDI08 TX polled without acquire barriers (`RingBuffer::sizeSameThread`, glue patch) | c30aa221 | 59.1% |

(mnm-bench, normal priority, pinned; the interpreter was ~225% on the same measure.)

**Realistic load (2026-09-27): every machine's p99 is under 100%.** `mnm-spikes` with `MNM_PACE=1`
(sleeps to each 128-frame deadline, like an audio thread), `SCHED_FIFO` 70, CPU 3. Measured with the
user's normal background load running: a JV-880 emulator in MPC using most of another core, and
MockbaMod's capture script run once a second.

| machine | mean | p99 (128 fr) | p99 (512 fr) | max |
|---|---|---|---|---|
| DPRO DDRW (heaviest) | 66.3% | 93.0% | 89.3% | 103.6% |
| DPRO DENS | 66.6% | 91.5% | 87.9% | 105.4% |
| RINGMOD | 64.8% | 88.9% | 86.5% | 101.1% |
| REVERB / SID 6581 | 61.6% | 87.2% / 83.8% | 83.6% / 81.0% | 97.4% / 92.4% |
| lightest (GND SIN) | 38.0% | 54.0% | 51.6% | 60.8% |

Method notes, so these numbers get reproduced properly:
- **Don't benchmark `SCHED_FIFO` without pacing.** A FIFO thread that never sleeps hits RT throttling
  (`sched_rt_runtime_us` 950000/1000000 on the Force), which shows as 14-block bursts of ~1.5x-slow
  blocks once per wall-clock second.
- **Even paced, a ~40 ms 1.3x slowdown recurs once a second**, from the background (a system-wide
  `perf record -a` shows `capture.sh`/`mount`/`mkdir` spawned every second at the same phase, plus
  `jv880-emu` running continuously). This is what sets p99 on this device today.
- **The slowest blocks execute the same number of DSP instructions as the rest** (`slow1%` = 1.00x
  in mnm-spikes), so all the variance is environmental.
- **Where the time goes now:** `prof.sh` (a `-g` build profiled on the device, samples mapped to
  inlined source functions with `addr2line -i`) is the tool that found the last four wins. Hot now:
  - `alu_mpyT` 7.5%
  - `updateAddressRegister` fast path 7% (memory read-modify-write of R registers)
  - `limit_transfer` 4%
  - `isPeripheralAddress` 3%
  - SR bit set/clear/toggle ~6% combined
  - the outer dispatch (runUntilTx + execRecompiled + peripheral tick) ~8%

Lessons:
- **opcodeanalysis' register masks are incomplete.** Some MAC/MPY entries report no X/Y source
  registers at all. Trusting them to reorder parallel moves broke 10 machines, which `mnm-golden`
  caught. Decide safety from opcode bits (move direction W) and from handler names, never from those
  masks alone. The dead-CCR pass also has name-based barriers for this reason.
- **p99 spikes are not DSP work.** `mnm-spikes` (per-block wall time and DSP instruction count):
  the slowest 1% of blocks execute exactly the average instruction count. At normal priority, the
  benchmark is preempted by other processes (node servers, VNC, MPC). Under `SCHED_FIFO` 70 pinned
  to CPU 3, which is how an audio thread really runs, every machine's mean drops ~25%. Heaviest
  machines:
  - DPRO DDRW: mean 84%, p99 122%
  - DPRO DENS: mean 83%, p99 117%
  - RINGMOD: mean 82%, p99 120%
  - REVERB: mean 75%, p99 113%
  - Everything else: mean <= 70%.

  mnm-bench's numbers (normal priority) overstate the real load. The remaining p99 is
  micro-architectural (cache/TLB, shared L2 with MPC's cores), so the VST wrapper should run the DSP on
  its own RT thread with a buffer of lookahead, where the mean is what counts.

## Stage 3 conclusion (2026-09-27)

**Gate met in substance: the heaviest machine is at ~57-67% of one core** (FIFO unpaced / paced with
background load), p99 <= 93% for every machine, bit-exact. The written target was 50-60%; DDRW paced is
67%, the rest are at or under it. The runtime-JIT plan (Stages 2-4 as first written) is superseded by
static recompilation plus targeted interpreter fixes. Final step table:

| | mnm-bench avg (normal prio) | heaviest, paced RT mean / p99 |
|---|---|---|
| interpreter | ~225% | (not real-time) |
| recompiler v1 | 113% | |
| + Stage 3 | **57.6-58.4%** | **67.1% / 92.9% (DPRO DDRW)** |

Last two steps: `isPeripheralAddress` threshold compare (1dce4d55, 58.1% -> 57.6%); the dispatch stats
counter moved behind `DSP56K_RECOMP_STATS` (no measurable effect).

What's left if more headroom is ever needed (remaining hot spots per `prof.sh`):
- Mode-bit reads from SR (SM saturation, S0/S1 scaling, SC) on every operation. They can't be cached
  across a block because CCR updates write the same SR word. Fix: split MR/CCR storage, or specialise
  blocks by mode with a guard at entry.
- 48/56-bit register half updates with 64-bit masks (`loword`/`hiword`), and address-register
  read-modify-writes through memory. The real fix is keeping DSP registers in CPU registers across a
  block, i.e. rewriting the hot handlers for the generator (a true Stage 3 codegen).
- Block chaining (skipping dispatch between fall-through blocks): ~8% of time is outer dispatch. Risk: it
  changes interrupt/TX-poll granularity, so it needs the golden check.
- Dead-CCR across loop iterations (peel the last iteration); more dead-CCR variants.

Open items for the port itself:
- **Coverage:** discovery uses mnm-golden's script. Code paths it never runs (other parameters, patterns,
  machine switching) fall back to the interpreter: correct, but slower. Before shipping, trace a richer
  workload (all parameters and ranges, note ranges, LFO modes, switching) and merge traces.
- **Legal/distribution:** the generated code embeds firmware opcode words, so the plugin with
  recompiled blocks can't be distributed publicly. Build it per user from their own OS `.syx` (Docker
  pipeline), or ship the generator and have users run it.
- **The armhf plain interpreter still differs from x86 on the 7 effect machines** (unexplained; handler
  resolution ruled out). The recompiled build matches x86. It matters only for code the recompiler falls
  back on.
- VST wrapper: run the DSP on its own `SCHED_FIFO` thread (pinned, one buffer of lookahead); the
  p99-vs-window data above supports that.


## Coexistence with the rest of MPC (2026-09-27)

Question: can a Monomodule instance (one voice, one machine, as upstream designs it) run on the Force while it
keeps running other tracks and plugins? Setup on the Force (which had your normal add-ons running):
- MPC has one `AudioWorker` per core: SCHED_RR priority 20, each pinned to its own core (0-3). MPC's main/UI
  threads are on core 0 and its background/file threads mostly on core 1. `Audio Processing` runs on core 3.
- The current project barely loads the workers (~1%, 4%, 1%, 0.2% of cores 0-3).
- Your `mpc-vst-jv880` runs a `jv880-emu` thread inside MPC at SCHED_FIFO **45** on cores 0-2, ~20% of a core.
- Core 3 has no JV-880 and the audio-DMA interrupt; per-core single-engine results are near-identical
  (0.3-1.5% of blocks over deadline for the heaviest machine on any core).
- The kernel is PREEMPT_RT, governor `performance`, no cpuidle driver. Each engine costs 62 MB PSS (2 GB total).

**One engine per core (cores 1,2,3), each engine's own processing time** (mean% / p99% of the 2902 us block):

| other load per core, engine priority | DDRW 1 / 2 / 3 engines | SWAVE SAW 1 / 2 / 3 engines |
|---|---|---|
| none, FIFO 25 | 68/88, 65/89, 65/92 | 47/63, 46/64, 47/66 |
| 25% RR-20, FIFO 25 (above MPC workers) | 67/88, 64/87, 65/90 | 47/63, 46/64, 47/65 |
| 50% RR-20, FIFO 25 | 59/84, 60/83, 63/86 | 44/60, 44/61, 45/65 |
| 25% RR-20, FIFO **15** (below workers) | 98/151, 95/143, 111/175 (30% / 19% / 95% of blocks late) | 50/94, 55/93, 76/110 |
| 50% RR-20, FIFO 15 | 166/263 (all late) | 106/181 (70% late) |

So **N instances need N separate cores, and the DSP thread must run above MPC's AudioWorkers (priority > 20).**
Below them, even 25% background load causes misses. More engines than free cores is not an option for the
heavy machines (two engines on one core would need >130%); two typical ones (2 x 47%) would only just fit.

**Cost to the other work when the engine runs above it** (one engine, FIFO 25, on the same core as RR-20 work
that does a fixed amount of work per 2902 us period; percentage of the other work's periods that finish late):

| other load on that core | DPRO DDRW (66%) | SWAVE SAW (46%) | GND SIN (38%) |
|---|---|---|---|
| 10% | 0.8% | 0.03% | 0 |
| 20% | 1.6% | 0 | 0 |
| 30% | 6.8% | 0.16% | 0 |
| 40% | 2.9% | 0.07% | 0.09% |
| 50% | 25% (worst 53 ms) | 0.07% | 0 |

Reading it: typical machines coexist with up to ~50% other real-time work on their core. The heavy ones
(DPRO DDRW/DENS, RINGMOD, REVERB, SID) need a core mostly to themselves: other work is fine up to ~20% and
degrades past ~30%. The pattern is that the engine's slowest blocks (p99 87%, max ~115%) hold the core for
milliseconds, and any other work waking during them runs late.

Harness pitfalls found on the way (all cost me time, so noting them):
- **Makespan vs per-engine time.** `mnm-bench --engines` used to report only the block's makespan, which
  includes waking an unpinned CFS coordinator thread. With engines on core 1 plus another core that added
  ~25% of a period and looked like 33% deadline misses; each engine's own time was fine (mean 65%, p99 92%).
  Trust the "slowest worker's own time" line.
- **SCHED_FIFO below MPC's workers** looks like a performance problem when it's a priority problem.
- **Compute-only load on other cores does not slow an engine** (it measured *faster*, 60% vs 69%, with
  burners on other cores; not understood, possibly the DRAM/interconnect staying at a higher clock).

Not measured / open:
- How MPC actually calls a plugin: on its AudioWorker (RR 20) inside the audio callback, or on threads the
  plugin creates. Everything above assumes the plugin owns a FIFO thread above priority 20. A plugin doing
  the DSP inside `process()` on an AudioWorker at 66% of a core would leave that core's worker only ~34%.
- Real MPC projects (this used a synthetic fixed-work-per-period load, with the current near-idle project).
- The cost of idle-voice skipping (would cut the load of silent tracks; not implemented).
- Starvation risk: a FIFO thread that overruns starves the AudioWorker on its core (RT throttling caps it at
  95% per second). The wrapper needs a bailout, e.g. skip a block and output silence when behind.
