#pragma once

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#	define HAVE_SSE
#endif

#if defined(_M_X64) || defined(__x86_64__) || defined(__x86_64) || defined(__amd__64__)
#	define HAVE_X86_64
#endif

#if defined(__aarch64__) || defined(__ARM_ARCH_8) || defined(_M_ARM64)
#	define HAVE_ARM64
#endif

// arm32 branch (sd88me/dsp56300): 32-bit ARM (armv7, e.g. Akai Force/MPC) has no JIT backend yet -- asmjit
// has no A32 support and this fork's own ARM32 JIT (docs/ARM32_JIT.md) is a work in progress. Until it
// lands, the JIT sources still compile (DSP embeds a Jit object unconditionally) against asmjit's x86-64
// emitter types, which are host-independent to compile against; g_jitSupported (dspconfig.h) stays false,
// so dsp.h's exec() always takes the interpreter path and no JIT code is ever generated or run.
#if defined(__arm__) && !defined(__aarch64__)
#	define DSP56K_NO_JIT_RUNTIME
#	define HAVE_X86_64
#endif
