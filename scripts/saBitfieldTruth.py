"""Capture EXTRACT/EXTRACTU/INSERT ground truth in Sixteen-bit Arithmetic mode.

Drives the Freescale reference simulator (sim56300) over the control word space and writes
truth.json, from which unittests_sa_bitfield.h is generated. See doc/sixteenBitArithmetic.md.

THE HARNESS RULE: `change <reg>` while SR.SA is set is itself remapped by the simulator, so
every register is written with SA OFF, SA is switched on only to execute, and switched off
again before the result is read back. Getting this wrong silently measures something else -
it cost a full day and a set of wrong conclusions once already.

Usage:
    python saBitfieldTruth.py emit          # writes chunk_*.in
    for f in chunk_*.in; do simrun < $f > ${f%.in}.log; done
    python saBitfieldTruth.py parse         # writes truth.json

The chunking is not cosmetic: the simulator segfaults on a large macro, and also on a handful
of individual signed `extract` cases, so a crash must cost only its own small chunk.
"""

import subprocess, re, sys, os

SA_ON  = '$c20300'
SA_OFF = '$000300'

PATTERNS = [
    # (name, a, x1, x0-unused-here)
    ('p0', '$ab123400567800', '$abcdef'),
    ('p1', '$8090a0b0c0d0e0', '$8000ff'),
]

WIDTHS  = list(range(64))
OFFSETS = list(range(64))

def cases():
    for pname, a, x1 in PATTERNS:
        for w in WIDTHS:
            for off in OFFSETS:
                imm = (w << 8) | off          # immediate layout: width 13-8, offset 5-0
                reg = (w << 16) | (off << 8)  # register  layout: width 21-16, offset 13-8
                for op, dst, operands in (('extractu','b','a,b'), ('extract','b','a,b'), ('insert','a','x1,a')):
                    yield dict(pat=pname, a=a, x1=x1, op=op, dst=dst, w=w, off=off,
                               form='imm', ctl=imm, ins=f'{op} #>${imm:06x},{operands}')
                    yield dict(pat=pname, a=a, x1=x1, op=op, dst=dst, w=w, off=off,
                               form='reg', ctl=reg, ins=f'{op} x0,{operands}')

def macro(cs):
    out = []
    for c in cs:
        out.append(f"change sr {SA_OFF}")
        out.append(f"change a {c['a']}")
        out.append("change b $00000000000000")
        out.append(f"change x1 {c['x1']}")
        out.append(f"change x0 ${c['ctl']:06x}")
        out.append(f"change sr {SA_ON}")
        out.append(f"asm p:$100 {c['ins']}")
        out.append("change pc $100")
        out.append("step")
        out.append(f"change sr {SA_OFF}")
        out.append(f"display {c['dst']}")
    return "\n".join(out) + "\n"

RE_ASM = re.compile(r'^asm p:\$100 (.+?)\s*$')
RE_SR  = re.compile(r'sr=\{?\$([0-9a-f]{6})\}?')
RE_VAL = re.compile(r'^\s+([ab])=\s+\{?\$([0-9a-f]{14})\}?')

def parse(log, cs, strict=True):
    blocks, cur = [], None
    for line in log.splitlines():
        if RE_ASM.match(line):
            if cur is not None: blocks.append(cur)
            cur = {'sr': None, 'vals': []}
            continue
        if cur is None: continue
        m = RE_SR.search(line)
        if m and cur['sr'] is None: cur['sr'] = m.group(1)
        for m in RE_VAL.finditer(line): cur['vals'].append((m.group(1), m.group(2)))
    if cur is not None: blocks.append(cur)
    if len(blocks) != len(cs):
        if strict: sys.exit(f"parse mismatch: {len(blocks)} blocks for {len(cs)} cases")
        n = min(len(blocks), len(cs)); blocks, cs = blocks[:n], cs[:n]
    out = []
    for c, b in zip(cs, blocks):
        vals = [v for r, v in b['vals'] if r == c['dst']]
        if not vals or b['sr'] is None:
            if strict: sys.exit(f"incomplete block for {c['ins']}")
            continue
        c = dict(c); c['result'] = vals[-1]; c['sr'] = b['sr']
        out.append(c)
    return out

CHUNK = 40

if __name__ == '__main__':
    import sys, json, glob, os
    cs = list(cases())
    chunks = [cs[i:i+CHUNK] for i in range(0, len(cs), CHUNK)]
    if sys.argv[1] == 'emit':
        for f in glob.glob('chunk_*.in') + glob.glob('chunk_*.log'): os.remove(f)
        for i, ch in enumerate(chunks):
            open(f'chunk_{i:04d}.in','w').write(macro(ch))
        print(f"cases={len(cs)} chunks={len(chunks)}")
    else:
        res, missing = [], []
        for i, ch in enumerate(chunks):
            p = f'chunk_{i:04d}.log'
            try:
                res += parse(open(p).read(), ch, strict=False)
            except Exception:
                missing.append(i)
        json.dump(res, open('truth.json','w'), indent=1)
        print(f"parsed={len(res)} of {len(cs)}   incomplete chunks={missing}")
