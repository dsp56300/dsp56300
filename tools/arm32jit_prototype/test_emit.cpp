// Correctness test for the Thumb-2 emitter, run under qemu-arm on the real target ABI: generate a
// block that calls a plain function twice with baked-in (this, op) pairs, exactly the shape Stage 2
// needs, and check the resulting state matches calling the same function directly in C++.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include "arm32asm.h"

struct State { int value; };

// noinline + extern "C"-like stability so the compiler can't cheat by inlining/optimizing the calls
// this test's generated code is supposed to be making instead.
extern "C" __attribute__((noinline)) void handlerAdd(State* s, uint32_t op) { s->value += int(op); }
extern "C" __attribute__((noinline)) void handlerMul(State* s, uint32_t op) { s->value *= int(op); }

typedef void (*BlockFn)(void);

int main() {
    State real{5};
    handlerAdd(&real, 3);
    handlerMul(&real, 2);
    // expected: (5+3)*2 = 16

    Arm32Asm a;
    a.push_r4_lr();
    State gen{5};
    a.load32(4, uint32_t(uintptr_t(&gen)));       // r4 = &gen (kept resident across calls)
    a.mov(0, 4);                                   // r0 = this
    a.load32(1, 3);                                // r1 = op
    a.load32(2, uint32_t(uintptr_t(&handlerAdd))); // r2 = handler addr
    a.blx(2);
    a.mov(0, 4);
    a.load32(1, 2);
    a.load32(2, uint32_t(uintptr_t(&handlerMul)));
    a.blx(2);
    a.pop_r4_pc();

    void* buf = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) { std::perror("mmap"); return 1; }
    std::memcpy(buf, a.code.data(), a.sizeBytes());
    // Thumb functions are called via an address with bit0 set.
    auto fn = reinterpret_cast<BlockFn>(reinterpret_cast<uintptr_t>(buf) | 1);
    fn();

    std::printf("expected=%d generated=%d %s\n", real.value, gen.value, real.value == gen.value ? "PASS" : "FAIL");
    return real.value == gen.value ? 0 : 1;
}
