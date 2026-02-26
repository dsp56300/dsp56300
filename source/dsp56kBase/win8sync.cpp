#include "win8sync.h"

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOSERVICE
#include <Windows.h>

namespace dsp56k
{
	namespace
	{
		bool g_initDone = false;
	}

	Win8Sync::WaitOnAddressFunc Win8Sync::m_funcWait = nullptr;
	Win8Sync::WakeByAddressFunc Win8Sync::m_funcWakeOne = nullptr;
	Win8Sync::WakeByAddressFunc Win8Sync::m_funcWakeAll = nullptr;

	void Win8Sync::loadWaitOnAddressFunctions()
	{
		if (g_initDone)
			return;

	    auto hModule = GetModuleHandleW(L"kernelbase.dll");

#if 0
		{
			// if compilation fails here, it means that our declarations of function pointer in the header file are wrong and need to be adjusted
			m_funcWait = WaitOnAddress;
			m_funcWakeOne = WakeByAddressSingle;
			m_funcWakeAll = WakeByAddressAll;
		}
#endif

		if (hModule)
	    {
	        m_funcWait = reinterpret_cast<WaitOnAddressFunc>(GetProcAddress(hModule, "WaitOnAddress"));
	        m_funcWakeOne = reinterpret_cast<WakeByAddressFunc>(GetProcAddress(hModule, "WakeByAddressSingle"));
	        m_funcWakeAll = reinterpret_cast<WakeByAddressFunc>(GetProcAddress(hModule, "WakeByAddressAll"));
	    }

		g_initDone = true;
	}

	bool Win8Sync::isSupported()
	{
		loadWaitOnAddressFunctions();
		return m_funcWait != nullptr;
	}
}

#endif
