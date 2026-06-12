#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOSERVICE
#include <Windows.h>

// Verify that dsp56k_SrwLock (declared in mutex.h) is layout-compatible
// with the real SRWLOCK. If these fail, mutex.h must be updated.
static_assert(sizeof(SRWLOCK) == sizeof(void*), "SRWLOCK is not pointer-sized");
static_assert(alignof(SRWLOCK) == alignof(void*), "SRWLOCK alignment differs from void*");

#endif
