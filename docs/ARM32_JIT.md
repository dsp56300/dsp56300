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
- Device confirmed reachable this session (`ssh root@192.168.1.44` -- armv7l). Not yet used for
  this stage's own timing measurement.
- Bail-out gate (unchanged from the original plan): if this isn't at least ~1.3x faster than the
  interpreter for that loop on-device, dispatch isn't where the time goes and full native codegen
  (Stage 3) likely won't pay for itself either -- stop.

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
