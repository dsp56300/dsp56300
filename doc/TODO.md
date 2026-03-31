# DSP56300 TODO

## Assembler

### Parallel instructions
The assembler cannot encode parallel instructions (ALU operation + data move in a single word).

- [ ] ALU + short immediate move: `sub x0,a #$3c,x0`
- [ ] ALU + register move: `tfr x0,a a0,x1`, `tfr y0,a a,b`
- [ ] ALU + ifcc conditional: `add b,a ifeq`, `cmp a,b ifge.u`
- [ ] ALU + memory move: `mac y0,x0,a y:(r5)+,y0`
- [ ] Movexr (X memory + register): `move x:(r2)+,a b,y0`
- [ ] Moveyr (register + Y memory): `move b,x0 y:(r2)+,a`
- [ ] Movexr_A / Moveyr_A: `move a,x:(r1) x0,a`, `move b,y:(r6) y0,b`
- [ ] Movexy (dual X+Y memory): `move x:(r2)+,a a,y:(r6)+`

### L-space (48-bit) moves
- [ ] Movel_ea: `move l:(r0),ab`, `move l:(r0),b`, `move l:(r1)+,x`
- [ ] Movel_aa: `move l:$3,b`, `move l:<$3,ab`, `move l:<$4,y`
- [ ] Movel write: `move x,l:(r1)`, `move a,l:(r3)`

### Movec (control register) gaps
- [ ] Movec_S1D2 with M registers: `move x0,m1`
- [ ] Movec_S1D2 with VBA/EP: `move y1,vba`, `move ep,x1`
- [ ] Movec_xx to LA: `move #$55,la`

### Movep (peripheral) gaps
- [ ] Movep_SXqq: `movep y1,x:<<$ffff84`
- [ ] Movep_SYqq: `movep y:<<$ffff86,b`
- [ ] Movep_Spp: `movep y:<<$ffffc5,y1`

### Extractu
- [ ] Register form: `extractu x0,a,b`
- [ ] Immediate form: `extractu #$8028,a,a`

### Encoding ambiguities
These instructions assemble to valid but different opcodes than some firmware uses:
- [ ] `tfr a,b` assembles as `move a,b` (24-bit saturating) instead of Tfr (56-bit). JJJ=0 and JJJ=1 both map to "other accumulator" in the decoder.
- [ ] `tne a,b` with JJJ<4 is rejected by assembler (JJJ<4 conflicts with parallel encoding). Assembler only produces JJJ>=4.
- [ ] `mac`/`mpy` with specific QQQ values: assembler may pick different source register pair encoding.

## Unimplemented instructions

### Both JIT and interpreter (28 instructions)

#### ALU
- [ ] ADC (add with carry)
- [ ] SBC (subtract with carry)
- [ ] CMPU (compare unsigned)
- [ ] MACI (MAC with long immediate)
- [ ] MACRI (MACR with long immediate)
- [ ] MPYRI (MPYR with long immediate)
- [ ] EOR xxxx (EOR with long immediate)
- [ ] MERGE (merge split fields)
- [ ] VSL (Viterbi shift left)
- [ ] NORM (normalize iteration)
- [ ] EXTRACT S1,S2 (extract bitfield, register)
- [ ] EXTRACT Co,S2 (extract bitfield, immediate)

#### Loop control
- [ ] DO FOREVER
- [ ] DOR aa (DO relative, absolute address)
- [ ] DOR FOREVER
- [ ] REP ea (repeat with effective address)
- [ ] REP aa (repeat with absolute address)

#### Move
- [ ] LRA Rn (load relative address, register variant)
- [ ] MOVEM aa (move program memory, absolute address)
- [ ] MOVEP ea,qq (peripheral, EA to I/O short)

#### Control flow
- [ ] BRKcc (break loop on condition)
- [ ] ILLEGAL (illegal instruction trap)
- [ ] TRAP (software trap)
- [ ] TRAPcc (conditional trap)

#### System / pipeline
- [ ] PFLUSH (pipeline flush)
- [ ] PLOCKR (pipeline lock relative)
- [ ] PUNLOCK (pipeline unlock)
- [ ] PUNLOCKR (pipeline unlock relative)

## Unit test gaps

### Addressing mode variants not tested
The core instruction logic is tested, but some addressing modes lack explicit unit tests:
- [ ] Bit-test-and-branch ea mode: `brclr/brset/bsclr/bsset` with effective address
- [ ] Bit-test-and-jump ea mode: `jclr/jset/jsclr/jsset` with effective address
- [ ] Bchg/Bset/Btst with ea and qq addressing

### System instructions (untestable in unit test context)
- Stop, Wait, Reset halt or block the DSP
- Rti requires interrupt context
- Debug/Debugcc are no-ops in production

## JIT optimizer

### Potential improvements
- [ ] Constant propagation across vector/SIMD registers (currently GP-only)
- [ ] DCE across conditional branches (currently conservative, sets all live)
- [ ] Remove asmjit ARM64 flag RW workarounds after asmjit update (see EMU-62)

## asmjit update (EMU-62)

Update from commit 3577608 to latest requires API migration due to v1.17 breaking changes.
See YouTrack EMU-62 for detailed change list.
