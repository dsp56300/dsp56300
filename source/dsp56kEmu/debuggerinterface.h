#pragma once

#include "types.h"

namespace dsp56k
{
	class DSPThread;
	class JitBlockRuntimeData;
	class JitDspMode;
	class DSP;

	class DebuggerInterface
	{
	public:
		explicit DebuggerInterface(DSP& _dsp);
		virtual ~DebuggerInterface();

		DSP& dsp() { return m_dsp; }
		const DSPThread* dspThread() const { return m_dspThread; }

		void setDspThread(const DSPThread* _thread) { m_dspThread = _thread; }

		virtual void onAttach();
		virtual void onDetach();

		bool isAttached() const { return m_attached; }

		virtual void onJitBlockCreated(const JitDspMode& _mode, const JitBlockRuntimeData* _block) {}
		virtual void onJitBlockDestroyed(const JitDspMode& _mode, TWord _pc) {}
		virtual void onProgramMemWrite(TWord _addr) {}
		virtual void onExec(TWord _addr) {}
		virtual void onMemoryWrite(EMemArea _area, TWord _addr, TWord _value) {}
		virtual void onMemoryRead(EMemArea _area, TWord _addr) {}
		virtual void onDebug() {}

	private:
		DSP& m_dsp;
		bool m_attached = false;
		const DSPThread* m_dspThread = nullptr;
	};
}
