// Static recompilation, step 1: trace every DSP instruction the interpreter executes while running mnm-golden's
// exact workload on every machine, and write what the generator (recomp_gen.py) needs to build basic blocks.
// Needs a dsp56300 build with -DDSP56K_RECOMP_DISCOVERY (the pre-execution hook) and MNM_DSP_INTERP=1.
//
//   mnm-recomp-discover <os.syx> <out.txt>
//
// Output lines:
//   I pc wordA wordB len kind parallel opOff moveOff aluOff count moveAB readsPC   one per distinct (pc, wordA, wordB)
//       kind: 0 = plain, 1 = ends its block (branch/return/loop or mode register write; PC checked after it),
//             2 = never recompiled (DO/REP/WAIT/IFcc, or not resolved); offsets are handler addresses
//             relative to the binary's load base (map them with nm -C of this same binary)
//       moveAB: a parallel instruction whose move reads or writes an accumulator its ALU part writes (needs the latch);
//       readsPC: the instruction reads PC (the generated code must set reg.pc/pcCurrentInstruction first)
//   E pc      an entry point: reached other than by falling through from the previous instruction
//   L la      a DO loop end address seen while the loop was active
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <tuple>
#include <vector>
#include <dlfcn.h>

#include "MonoVoice.h"
#include "firmware/Firmware.h"
#include "host/Machines.h"
#include "dsp56kEmu/dsp.h"
#include "dsp56kEmu/opcodes.h"
#include "dsp56kEmu/opcodeanalysis.h"

using namespace mnm;
using namespace dsp56k;

namespace {
struct Seen { uint64_t count = 0; };
std::map<TWord, uint64_t> g_runCount;        // this machine's executed PCs
std::set<TWord> g_entries, g_loopEnds;
TWord g_expectedNext = 0xffffffff;
Opcodes g_ops;

TWord lengthAt(DSP* d, TWord pc) {
    const TWord a = d->memory().get(MemArea_P, pc);
    Instruction ia = Nop, ib = Invalid;
    if (a) g_ops.getInstructionTypes(a, ia, ib);
    const auto len = Opcodes::getOpcodeLength(a, ia, ib);
    return len ? len : 1;
}

void hook(DSP* d, TWord pc) {
    ++g_runCount[pc];
    if (pc != g_expectedNext) g_entries.insert(pc);
    if (d->regs().sr.var & SR_LF) g_loopEnds.insert(TWord(d->regs().la.var));
    g_expectedNext = pc + lengthAt(d, pc);
}

uintptr_t off(void* p) { Dl_info i{}; return p && dladdr(p, &i) ? uintptr_t(p) - uintptr_t(i.dli_fbase) : 0; }

using Key = std::tuple<TWord, TWord, TWord>;
struct Info { TWord len; int kind; bool parallel; size_t op, mv, alu; uint64_t count; bool moveAB, readsPC; };
std::map<Key, Info> g_infos;

void collect(DSP& d) {
    for (const auto& [pc, n] : g_runCount) {
        const TWord a = d.memory().get(MemArea_P, pc), b = d.memory().get(MemArea_P, pc + 1);
        const Key k{pc, a, b};
        auto it = g_infos.find(k);
        if (it != g_infos.end()) { it->second.count += n; continue; }
        Instruction ia = Nop, ib = Invalid;
        if (a) g_ops.getInstructionTypes(a, ia, ib);
        const TWord len = Opcodes::getOpcodeLength(a, ia, ib);
        const auto ri = d.getRecompInfo(pc);
        const auto flags = Opcodes::getFlags(ia, ib);
        RegisterMask written = RegisterMask::None, read = RegisterMask::None;
        Opcodes::getRegisters(written, read, a, ia, ib);
        constexpr auto ctrl = RegisterMask::PC | RegisterMask::LA | RegisterMask::LC | RegisterMask::SSH | RegisterMask::SSL |
                              RegisterMask::SP | RegisterMask::SC | RegisterMask::EP | RegisterMask::SZ | RegisterMask::EMR |
                              RegisterMask::MR | RegisterMask::OMR;
        int kind = 0;
        if (!ri.resolved || (flags & (OpFlagLoop | OpFlagRepDynamic | OpFlagRepImmediate)) || ia == Wait || ia == Ifcc ||
            ia == Ifcc_U || ib == Ifcc || ib == Ifcc_U)
            kind = 2;
        else if ((flags & (OpFlagBranch | OpFlagPopPC)) || (written & ctrl) != RegisterMask::None)
            kind = 1;
        bool moveAB = true;
        if (ri.parallel) {
            RegisterMask mw = RegisterMask::None, mr = RegisterMask::None;
            RegisterMask aw = RegisterMask::None, ar = RegisterMask::None;
            Opcodes::getRegisters(mw, mr, a, ib, Invalid);   // the move part only (ib is the move of a parallel pair)
            Opcodes::getRegisters(aw, ar, a, ia, Invalid);   // the ALU part only
            // the latch matters only for an accumulator the ALU writes and the move also reads or writes
            auto touches = [](RegisterMask m, RegisterMask acc) { return (m & acc) != RegisterMask::None; };
            moveAB = (touches(aw, RegisterMask::A) && touches(mw | mr, RegisterMask::A)) ||
                     (touches(aw, RegisterMask::B) && touches(mw | mr, RegisterMask::B));
        }
        const bool readsPC = (read & RegisterMask::PC) != RegisterMask::None;
        g_infos[k] = {len ? len : 1, kind, ri.parallel, off(ri.op), off(ri.opMove), off(ri.opAlu), n, moveAB, readsPC};
    }
    g_runCount.clear();
}
}

int main(int argc, char** argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: mnm-recomp-discover <os.syx> <out.txt>\n"); return 2; }
    const auto fw = fw::loadFirmware(argv[1]);
    DSP::s_recompTraceHook = &hook;
    constexpr int kChunk = 128, kChunks = 44100 * 3 / kChunk;

    for (const auto& def : host::kMachineDefs) {   // mnm-golden's script, unchanged
        MonoVoice v(fw);
        g_expectedNext = 0xffffffff;
        auto& h = v.host();
        h.setMachine(def.machine);
        const bool fx = host::isFxMachine(def.machine);
        h.setRouting(fx ? host::dspInputBits(host::FxInput::InpAB) : 0u);
        v.warmUp(8);
        h.setLfoParam(0, 0, 0); h.setLfoParam(0, 1, 0); h.setLfoParam(0, 6, 64); h.setLfoParam(0, 7, 90);
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> noise(-0.6f, 0.6f);
        std::vector<float> L(kChunk), R(kChunk), inL(kChunk), inR(kChunk);
        h.noteOn(fx ? 60 : 45);
        for (int c = 0; c < kChunks; ++c) {
            if (c % 8 == 0) {
                const int step = c / 8;
                const auto page = host::Page(step % 4);
                const int k = (step / 4) % 8;
                if (!(page == host::Page::AMP && k >= 2 && k <= 5 && fx)) h.setParam(page, k, (step * 37) % 128);
            }
            if (!fx) {
                if (c == kChunks / 3) h.noteOn(52);
                if (c == kChunks / 2) h.noteOn(57);
                if (c == 2 * kChunks / 3) h.noteOff();
            }
            if (fx) {
                const float g = c < kChunks / 2 ? 1.f : 0.f;
                for (int i = 0; i < kChunk; ++i) { inL[i] = g * noise(rng); inR[i] = g * noise(rng); }
                v.processFx(inL.data(), inR.data(), L.data(), R.data(), kChunk);
            } else {
                v.process(L.data(), R.data(), kChunk);
            }
        }
        collect(v.engine().dsp());
        std::fprintf(stderr, "%-11s traced, %zu distinct instructions so far\n", def.name, g_infos.size());
    }

    FILE* f = std::fopen(argv[2], "w");
    if (!f) { std::perror(argv[2]); return 1; }
    for (const auto& [k, i] : g_infos)
        std::fprintf(f, "I %06x %06x %06x %u %d %d %zx %zx %zx %llu %d %d\n", std::get<0>(k), std::get<1>(k), std::get<2>(k), i.len,
                     i.kind, int(i.parallel), i.op, i.mv, i.alu, (unsigned long long)i.count, int(i.moveAB), int(i.readsPC));
    for (auto e : g_entries) std::fprintf(f, "E %06x\n", e);
    for (auto l : g_loopEnds) std::fprintf(f, "L %06x\n", l);
    std::fclose(f);
    return 0;
}
