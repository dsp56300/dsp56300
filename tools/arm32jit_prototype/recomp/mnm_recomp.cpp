// Static-recompilation gate test (arm32 JIT investigation, after Stage 2 failed): compile Stage 1's sine-table
// loop body (P:$100091..$10009a) ahead of time into C++ that calls the interpreter's own handlers with constant
// opcodes, so GCC can inline and constant-fold them, then diff registers against the interpreter and time both.
//   mnm-recomp <os.syx> --gen        print the handlers the interpreter resolved (offsets for recomp_gen.py)
//   mnm-recomp <os.syx> [iters]      verify and time (needs a build with the generated dsp56k_recomp.inl)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>
#include <dlfcn.h>
#include "MonoVoice.h"
#include "firmware/Firmware.h"
#include "dsp56kEmu/dsp.h"

using namespace mnm;
using namespace dsp56k;

namespace {
constexpr TWord kStart = 0x100091, kEnd = 0x10009b;
// The loop body writes l:(r1) (r1 cycles 0..1, modulo 2) and y:(r0)+ (r0 from $14b801), so every run must also
// start from the same memory, not just the same registers -- otherwise the second run reads the first run's writes.
struct MemWord { EMemArea area; TWord addr; TWord val; };
std::vector<MemWord> g_mem;
void captureMemory(DSP& dsp) {
    for (auto area : {MemArea_X, MemArea_Y}) {
        for (TWord a = 0; a < 4; ++a) g_mem.push_back({area, a, dsp.memory().get(area, a)});
        for (TWord a = 0x14b7f8; a < 0x14b810; ++a) g_mem.push_back({area, a, dsp.memory().get(area, a)});
    }
}
void setSnapshot(DSP& dsp) {
    for (const auto& m : g_mem) dsp.memory().set(m.area, m.addr, m.val);
    auto& r = dsp.regs();
    r.x.var = 0x7ffffd885869;
    r.y.var = 0x800009c036d6;
    r.a.var = 0xff7fffffe19916;
    r.b.var = 0x00000000001ffe;
    r.r[0].var = 0x14b801;
    r.r[1].var = 0x000000;
    r.m[1].var = 0x000001;
    r.mMask[1] = 0x000001;
    r.mModulo[1] = 0x000002;
    r.sr.var = 0x088369;
    dsp.setPC(kStart);
}
uintptr_t off(void* p) { Dl_info i{}; return p && dladdr(p, &i) ? uintptr_t(p) - uintptr_t(i.dli_fbase) : 0; }
// full register dump, minus the cumulative instruction counter line (differs between runs by construction)
std::string regs(DSP& dsp) {
    std::stringstream s, out; dsp.dumpRegisters(s);
    for (std::string l; std::getline(s, l);) if (l.find("ictr") == std::string::npos) out << l << '\n';
    return out.str();
}
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: mnm-recomp <os.syx> [--gen | iters]\n"); return 2; }
    const auto fw = fw::loadFirmware(argv[1]);
    MonoVoice v(fw);
    auto& eng = v.engine();
    auto& dsp = eng.dsp();
    dsp.setInterpreterEnabled(true);
    captureMemory(dsp);

    setSnapshot(dsp);
    int nInstr = 0;
    while (dsp.getPC().toWord() < kEnd) {
        const TWord pc = dsp.getPC().toWord();
        dsp.execInterpreter();
        if (argc > 2 && std::string(argv[2]) == "--gen") {
            const auto ri = dsp.getRecompInfo(pc);
            std::printf("%06x %06x %06x %d %zx %zx %zx\n", pc, eng.peek(fw::Space::P, pc), eng.peek(fw::Space::P, pc + 1),
                        int(ri.parallel), size_t(off(ri.op)), size_t(off(ri.opMove)), size_t(off(ri.opAlu)));
        }
        ++nInstr;
    }
    if (argc > 2 && std::string(argv[2]) == "--gen") return 0;
    const auto refStr = regs(dsp);

    setSnapshot(dsp);
    if (!dsp.runRecompiled(kStart)) { std::fprintf(stderr, "no recompiled block in this build\n"); return 1; }
    const auto genStr = regs(dsp);
    const bool ok = refStr == genStr;
    if (!ok) std::printf("--- interpreter ---\n%s\n--- recompiled ---\n%s\n", refStr.c_str(), genStr.c_str());
    std::printf("%s (%d instructions)\n", ok ? "PASS: identical register state" : "FAIL: register state differs", nInstr);

    const int iters = argc > 2 ? std::atoi(argv[2]) : 2000000;
    auto now = [] { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; };
    double tReset = now();
    for (int k = 0; k < iters; ++k) setSnapshot(dsp);
    tReset = now() - tReset;
    double tInt = now();
    for (int k = 0; k < iters; ++k) { setSnapshot(dsp); for (int i = 0; i < nInstr; ++i) dsp.execInterpreter(); }
    tInt = now() - tInt - tReset;
    double tGen = now();
    for (int k = 0; k < iters; ++k) { setSnapshot(dsp); dsp.runRecompiled(kStart); }
    tGen = now() - tGen - tReset;
    const double n = double(iters) * nInstr;
    std::printf("interpreter: %.1f ns/instr\nrecompiled:  %.1f ns/instr\nspeedup:     %.2fx\n", tInt / n, tGen / n, tInt / tGen);
    return ok ? 0 : 1;
}
