#pragma once

namespace dsp56k
{
#ifdef _WIN32
	class Win8Sync
	{
	public:
		// not using Windows types on purpose here to prevent inclusion of Windows.h
		typedef int(__stdcall* WaitOnAddressFunc)(volatile void*, void*, unsigned long long, unsigned long);
		typedef void(__stdcall* WakeByAddressFunc)(void*);

		static void loadWaitOnAddressFunctions();
		static bool isSupported();

		static WaitOnAddressFunc getWaitOnAddressFunc() { loadWaitOnAddressFunctions(); return m_funcWait; }
		static WakeByAddressFunc getWakeOneFunc() { loadWaitOnAddressFunctions(); return m_funcWakeOne; }
		static WakeByAddressFunc getWakeAllFunc() { loadWaitOnAddressFunctions(); return m_funcWakeAll; }

	private:
		static WaitOnAddressFunc m_funcWait;
		static WakeByAddressFunc m_funcWakeOne;
		static WakeByAddressFunc m_funcWakeAll;
	};
#endif
}
