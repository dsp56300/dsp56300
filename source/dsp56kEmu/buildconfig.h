#pragma once

#if defined(_M_IX86) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__)
#	define HAVE_SSE
#endif

#if defined(_WIN64) || defined(__x86_64__) || defined(__x86_64) || defined(__amd__64__)
#	define HAVE_X86_64
#endif

#if defined(__aarch64__) || defined(__ARM_ARCH_8)
#	define HAVE_ARM64
#endif
