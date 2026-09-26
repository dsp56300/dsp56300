// Stage 2 proof: compile the sine-table loop body's 10 instructions (100091..10009a, fully
// reverse-engineered in Stage 1) into one hand-written Thumb-2 block, run it once, and diff full
// register state against running the same 10 instructions through the plain interpreter from the
// exact same starting state (the register snapshot captured at the real Stage-1 divergence point).
// x86-hosted with a raw byte emitter, run/tested for real only under qemu-arm / on the Force.
#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>
#include "MonoVoice.h"
#include "firmware/Firmware.h"
#include "dsp56kEmu/dsp.h"
#include "dsp56kEmu/opcodes.h"
#include "arm32asm.h"

using namespace mnm;
using namespace dsp56k;

namespace {
template <typename MFP> void* rawAddr(MFP f) {
    static_assert(sizeof(MFP) == 2 * sizeof(void*), "expected the non-virtual member-pointer layout {ptr, adjustment}");
    void* raw[2];
    std::memcpy(raw, &f, sizeof(raw));
    return raw[0];
}
void setSnapshot(DSP& dsp) {
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
    dsp.setPC(0x100091);
}
}

int main(int argc, char** argv) {
    { const char m[] = "main() reached\n"; write(2, m, sizeof(m) - 1); }
    if (argc < 2) { std::fprintf(stderr, "usage: mnm-arm32block <os.syx>\n"); return 2; }
    const auto fw = fw::loadFirmware(argv[1]);
    { const char m[] = "firmware loaded\n"; write(2, m, sizeof(m) - 1); }
    MonoVoice v(fw);
    { const char m[] = "MonoVoice constructed\n"; write(2, m, sizeof(m) - 1); }
    auto& eng = v.engine();
    auto& dsp = eng.dsp();
    dsp.setInterpreterEnabled(true);   // sizes m_opcodeCache; execInterpreter() indexes it unconditionally
    Opcodes ops;

    const uint32_t prepareOpAddr = uint32_t(uintptr_t(rawAddr(&DSP::prepareOp)));
    const uint32_t callInstructionAddr = uint32_t(uintptr_t(rawAddr(&DSP::callInstruction)));
    const uint32_t callParallelAddr = uint32_t(uintptr_t(rawAddr(&DSP::callParallel)));

    // --- compile P:$100091..$10009a into one block ---
    Arm32Asm a;
    a.push_r4_lr();
    a.load32(4, uint32_t(uintptr_t(&dsp)));   // r4 = dsp*, resident for the whole block

    constexpr TWord kStart = 0x100091, kEnd = 0x10009a + 1;
    TWord pc = kStart;
    int nInstr = 0;
    while (pc < kEnd) {
        const TWord wordA = eng.peek(fw::Space::P, pc);
        const TWord wordB = eng.peek(fw::Space::P, pc + 1);

        Instruction instA = Nop, instB = Invalid;
        if (wordA) ops.getInstructionTypes(wordA, instA, instB);
        const TWord len = ops.getOpcodeLength(wordA, instA, instB);
        const TWord pcAfterFirstWord = pc + 1;

        // r0=dsp, r1=opWordB, r2=pcAfterFirstWord; blx prepareOp
        a.mov(0, 4);
        a.load32(1, wordB);
        a.load32(2, pcAfterFirstWord);
        a.load32(3, prepareOpAddr);
        a.blx(3);

        if (instB == Invalid) {
            // single instruction (Nop, non-parallel, or a parallel opcode with only one slot filled)
            const auto handler = rawAddr(DSP::resolvePermutation(instA, wordA));
            a.mov(0, 4);
            a.load32(1, uint32_t(uintptr_t(handler)));
            a.load32(2, wordA);
            a.load32(3, callInstructionAddr);
            a.blx(3);
        } else {
            // parallel: instA=alu, instB=move -- callParallel(this, rawMove, rawAlu, op)
            const auto handlerMove = rawAddr(DSP::resolvePermutation(instB, wordA));
            const auto handlerAlu  = rawAddr(DSP::resolvePermutation(instA, wordA));
            a.mov(0, 4);
            a.load32(1, uint32_t(uintptr_t(handlerMove)));
            a.load32(2, uint32_t(uintptr_t(handlerAlu)));
            a.load32(3, wordA);
            a.load32(5, callParallelAddr);   // 4 args fill r0-r3; keep the callee address in a scratch reg
            a.blx(5);
        }
        std::printf("pc=%06x wordA=%06x wordB=%06x len=%u instA=%d instB=%d\n",
                     pc, wordA, wordB, len, int(instA), int(instB));
        pc += len ? len : 1;
        ++nInstr;
    }
    a.pop_r4_pc();
    std::printf("compiled %d instructions, %zu bytes\n", nInstr, a.sizeBytes());

    void* buf = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) { std::perror("mmap"); return 1; }
    std::memcpy(buf, a.code.data(), a.sizeBytes());

    // --- reference: interpreter, from the identical starting state ---
    setSnapshot(dsp);
    for (int i = 0; i < nInstr; ++i) dsp.execInterpreter();
    std::stringstream refSS; dsp.dumpRegisters(refSS);
    const auto refStr = refSS.str();

#if defined(__arm__)
    auto fn = reinterpret_cast<void (*)()>(reinterpret_cast<uintptr_t>(buf) | 1);   // Thumb: bit0 set
    setSnapshot(dsp);
    std::fprintf(stderr, "about to call generated block at %p (thumb entry %p)\n", buf, reinterpret_cast<void*>(fn));
    std::fflush(nullptr);
    fn();
    std::fprintf(stderr, "generated block returned\n");
    std::fflush(nullptr);
    std::stringstream genSS; dsp.dumpRegisters(genSS);
    const auto genStr = genSS.str();

    std::printf("--- interpreter ---\n%s\n--- generated block ---\n%s\n", refStr.c_str(), genStr.c_str());
    std::printf("%s\n", refStr == genStr ? "PASS: identical register state" : "FAIL: register state differs");
    return refStr == genStr ? 0 : 1;
#else
    std::printf("--- interpreter reference ---\n%s\n(x86 host build: not executing generated Thumb-2 code)\n", refStr.c_str());
    return 0;
#endif
}
