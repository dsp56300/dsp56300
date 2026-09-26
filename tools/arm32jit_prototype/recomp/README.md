# Static recompiler for dsp56300 on 32-bit ARM (Monomodule / Akai Force)

The dsp56300 program is translated ahead of time into C++: one `__attribute__((flatten))` function per basic
block (plus a whole-loop variant for single-block DO loop bodies), calling the interpreter's own handlers with
constant opcodes. It's compiled into `dsp.cpp`, where every handler can be inlined and constant-folded. Anything
not recompiled, or whose P memory no longer matches, runs on the interpreter. See `docs/ARM32_JIT.md` for the
design, the correctness rules and all measurements.

**The generated `dsp56k_recomp.inl` contains opcode words from the Monomachine OS firmware. Never commit or
distribute it** (`.gitignore` covers it). Build it from your own OS `.syx`, the same way Monomodule itself
needs your own OS file.

## Files
- `mnm_recomp_discover.cpp`: tracer (mnm-golden's workload, all 22 machines) that writes the instruction trace.
- `recomp_gen2.py`: trace + `nm -C` of the tracer binary -> `dsp56k_recomp.inl`.
- `schwung-monomodule-glue.patch`: glue changes for schwung-monomodule (DspEngine polls HDI08 TX without
  barriers) and the CMake targets for the tools here.
- `mnm_spikes.cpp`: per-block timing and DSP instruction counts. Options: `MNM_CPU`, `MNM_FIFO` (prio),
  `MNM_PACE=1` (sleep to each deadline like an audio thread), `MNM_DUMP=<file>`.
- `bench.py` + `devbench.sh`: interleaved, pinned median of mnm-bench on the Force.
- `prof.sh` + `profagg.py`: `-g` build profiled on the device, samples mapped to inlined source
  functions/lines.
- `mnm_recomp.cpp` + `recomp_gen.py`: the earlier single-loop gate test (superseded).

## Pipeline (Docker images as in `../toolchain-diff/`; x86 image = that plus build-essential)
1. Apply `schwung-monomodule-glue.patch` to schwung-monomodule and copy the `.cpp` tools into its
   `tools/bench/`.
2. x86 discovery build: `-DMNM_DSP56300_DIR=<this fork> -DCMAKE_CXX_FLAGS=-DDSP56K_RECOMP_DISCOVERY`, then:
   ```
   MNM_DSP_INTERP=1 mnm-recomp-discover os.syx disc.txt
   nm -C mnm-recomp-discover > nm.txt
   python3 recomp_gen2.py disc.txt nm.txt > <dir>/dsp56k_recomp.inl
   ```
3. Builds for x86 and armhf: `-DCMAKE_CXX_FLAGS="-DDSP56K_RECOMP -I<dir>"`.
4. Gate: `mnm-golden` with `MNM_DSP_INTERP=1` must match the x86 interpreter's hashes on all 22 machines,
   on x86 and on the device.
5. Runtime: always run with `MNM_DSP_INTERP=1` on armv7 (there is no runtime JIT; without it DspEngine
   takes the JIT path and crashes).
