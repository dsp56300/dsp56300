#ifdef _MSC_VER
#	include <Windows.h>
#else
#	include <signal.h>
#endif

namespace dsp56k
{
	void nativeDebugBreak()
	{
#ifdef _MSC_VER
			if(IsDebuggerPresent())
				DebugBreak();
#else
//			raise(SIGTRAP);
#endif
	}
}
