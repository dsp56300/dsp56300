# EXTRACT / EXTRACTU / INSERT in Sixteen-bit Arithmetic mode

Everything here is a fit to ground truth captured from the Freescale reference simulator over
the complete control word space - 48856 cases, both control forms, two data patterns, zero
mismatches. The family manual is not sufficient on its own: it describes the control *register*
layout and leaves the immediate form, the two different width fields and the INSERT offset bias
to be inferred, and inferring them wrongly is easy and was done here once before.

Regenerate the truth with `scripts/saBitfieldTruth.py`; the unit test table
`source/dsp56kEmu/unittests_sa_bitfield.h` is generated from it and must never be hand-edited.

## The accumulator view

In SA mode the accumulator is an 8 bit EXT with a 16 bit MSP and a 16 bit LSP. In terms of the
raw 56 bit register that is the uniform "a register holds its 16 bit value in bits 23-8" rule
applied to each half:

| part | raw bits | equivalently |
|------|----------|--------------|
| EXT  | 55-48    | A2           |
| MSP  | 47-32    | A1[23-8]     |
| LSP  | 23-8     | A0[23-8]     |

Together they form the 40 bit value the bitfield instructions operate on. A write through this
path clears the least significant byte of each half, which is the corruption the manual warns
about in section 3.4.

## The control word

One logical word carrying a width and an offset. A control **register** is read like any other
register in SA mode, so it holds that word in bits 23-8 and its fields sit 8 bits higher than an
**immediate**'s:

| form      | width  | offset |
|-----------|--------|--------|
| register  | 21-16  | 13-8   |
| immediate | 13-8   | 5-0    |

Register `$081000` and immediate `$000810` therefore give the identical result, while register
`$000810` is a width of zero and does nothing. Outside SA mode both forms use width 17-12 and
offset 5-0.

## EXTRACT / EXTRACTU

- width is 6 bits; offset addresses the 40 bit value directly and reads zero from bit 40 up
- the source is signed - EXT is its sign extension - so a field near the top picks up the sign
- EXTRACT sign-extends the field from its width, EXTRACTU does not
- a width beyond the 40 bit datapath (41 and above) yields no EXT at all

## INSERT

- width is only **5** bits, and is further clamped to the 16 bits a source register can supply
- the offset carries a **bias of 16**: a field below the bias inserts nothing at all, not even
  partially; one running off the top is simply truncated at 40 bits
- the source is read through the SA convention, its 16 bit value living in bits 23-8

Note the asymmetry: EXTRACT's offset has no bias and its width field is 6 bits, INSERT's offset
is biased by 16 and its width field is 5 bits. Both were measured, neither is a guess.

## Condition codes

No SA-specific handling is needed. Because the SA view is just the top of the 56 bit register
with holes in it, the ordinary definitions already produce the right answers: N is bit 55, Z is
the whole register being zero, U compares bits 47 and 46, and E asks whether bits 55-47 are all
equal. C and V are cleared. All four were confirmed against the captured CCR of every case.

## The harness trap

`change <reg>` in the simulator while SR.SA is already set is itself remapped, so the register
does not hold what was typed. Set registers with SA **off**, switch SA on only to execute, and
switch it off again before reading the result back. Measuring with SA enabled first produces a
self-consistent but entirely wrong picture.
