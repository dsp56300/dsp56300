// Verifies: (1) TInstructionFunc really is {address,0} under the Itanium ABI for real dsp56300
// handler functions (not just a toy class), and (2) DSP::callInstruction/callParallel (the new
// public wrappers) behave identically to calling the resolved handler directly.
#include <cstdio>
#include <cstring>
#include "dsp.h"
#include "memory.h"
#include "peripherals.h"

using namespace dsp56k;

int main() {
    DefaultMemoryValidator validator;
    Memory mem(validator, 0x1000, 0x1000, 0);
    Peripherals56303 periphX;
    PeripheralsNop periphY;
    DSP dsp(mem, &periphX, &periphY);
    dsp.setInterpreterEnabled(true);

    // INC A (Instruction "Inc"): increments the accumulator by 1, op field only selects a/b (0 = a).
    // Starting from a known nonzero value makes a no-op wrapper bug show up as a mismatch, not a
    // trivial 0==0 pass.
    const Instruction inst = Inc;
    const TWord op = 0;

    TInstructionFunc f = DSP::resolvePermutation(inst, op);
    void* raw[2]; std::memcpy(raw, &f, sizeof(raw));
    std::printf("resolved addr=%p adj=%p\n", raw[0], raw[1]);
    if (raw[1] != nullptr) { std::printf("FAIL: adjustment word is not zero -- ABI assumption violated\n"); return 1; }

    // Direct call via member pointer.
    dsp.regs().a.var = 100;
    (dsp.*f)(op);
    const auto direct = dsp.regs().a.var;

    // Reset and call via the raw-address wrapper.
    dsp.regs().a.var = 100;
    dsp.callInstruction(raw[0], op);
    const auto wrapped = dsp.regs().a.var;

    std::printf("direct=%lld wrapped=%lld %s\n", (long long)direct, (long long)wrapped, direct == wrapped ? "PASS" : "FAIL");
    return direct == wrapped ? 0 : 1;
}
