#pragma once

#include <memory>
#include <thread>

#include "debuggerJitState.h"

#include "../dsp56kEmu/debuggerinterface.h"

#include "debuggerState.h"
#include "dspExec.h"

namespace dsp56kDebugger
{
	class DebuggerListener;

	class Debugger : public dsp56k::DebuggerInterface
	{
	public:
		explicit Debugger(dsp56k::DSP& _dsp);
		~Debugger() override;

		// called by DSP thread
		void onJitBlockCreated(const dsp56k::JitDspMode& _mode, const dsp56k::JitBlock* _block) override;
		void onJitBlockDestroyed(const dsp56k::JitDspMode& _mode, dsp56k::TWord _pc) override;
		void onExec(dsp56k::TWord _addr) override;
		void onMemoryWrite(dsp56k::EMemArea _area, dsp56k::TWord _addr, dsp56k::TWord _value) override;
		void onMemoryRead(dsp56k::EMemArea _area, dsp56k::TWord _addr) override;
		void onDebug() override;

		JitState& jitState() { return m_jitState; }
		
		template<typename F, class... TArgs>
		void sendEvent(const F& _f, const TArgs&... args)
		{
			auto f = std::mem_fn(_f);

			const auto listeners = m_listeners;

			for (auto* l : listeners)
			{
				if(m_listeners.find(l) != m_listeners.end())
					f(*l, args...);
			}
		}

		void addListener(DebuggerListener* _listener);
		void removeListener(DebuggerListener* _listener);

		State& getState() { return m_state; }
		DspExec& dspExec() { return m_dspExec; }

		dsp56k::Disassembler& disasm() { return m_disasm; }

		dsp56k::TWord disasmAt(dsp56k::Disassembler::Line& _line, dsp56k::TWord _addr);
		dsp56k::TWord disasmAt(std::string& _dst, dsp56k::TWord _addr, bool _useIndent);
		dsp56k::TWord getBranchTarget(dsp56k::TWord _addr, dsp56k::TWord _pc);

		void setJitConfig(const dsp56k::JitConfig& _config);
		void destroyAllJitBlocks();
		bool getMemoryAddress(dsp56k::TWord& _addr, dsp56k::EMemArea& _area, const dsp56k::TWord& _memAddr);

	private:
		void windowRunner();

		dsp56k::Disassembler m_disasm;

		std::set<DebuggerListener*> m_listeners;
		std::unique_ptr<std::thread> m_windowRunner;

		State m_state;
		JitState m_jitState;

		DspExec m_dspExec;
	};
}
