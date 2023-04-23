#include "debugger.h"

#include "mainWindow.h"
#include "dsp56kEmu/jitblock.h"
#include "dsp56kEmu/jitdspmode.h"
#include "wx/app.h"

wxDEFINE_EVENT(dspUpdate, wxCommandEvent);

class DebuggerApp : public wxApp
{
public:
	DebuggerApp(dsp56kDebugger::Debugger& _debugger) : m_debugger(_debugger)
	{
	}

	bool OnInit() override
	{
		const auto res = wxApp::OnInit();
		m_win = new dsp56kDebugger::MainWindow(m_debugger);
		m_win->Show();
		return res;
	}

	int OnExit() override
	{
		m_win->Close();
		return wxApp::OnExit();
	}

private:
	dsp56kDebugger::Debugger& m_debugger;
	dsp56kDebugger::MainWindow* m_win = nullptr;
};

namespace dsp56kDebugger
{
	Debugger::Debugger(dsp56k::DSP& _dsp)
		: dsp56k::DebuggerInterface(_dsp)
		, m_disasm(_dsp.opcodes())
		, m_state(*this)
		, m_dspExec(*this)
	{
		const auto* p0 = _dsp.getPeriph(0);
		const auto* p1 = _dsp.getPeriph(1);

		if(p0) p0->setSymbols(m_disasm);
		if(p1) p1->setSymbols(m_disasm);

		m_windowRunner.reset(new std::thread([&]()
		{
			windowRunner();
		}));
	}

	Debugger::~Debugger()
	{
		auto* instance = wxApp::GetInstance();
		instance->Exit();
		m_windowRunner->join();
		m_windowRunner.reset();
	}

	void Debugger::onJitBlockCreated(const dsp56k::JitDspMode& _mode, const dsp56k::JitBlockRuntimeData* _block)
	{
		DebuggerInterface::onJitBlockCreated(_mode, _block);
		m_jitState.onJitBlockCreated(_mode, _block);
	}

	void Debugger::onJitBlockDestroyed(const dsp56k::JitDspMode& _mode, dsp56k::TWord _pc)
	{
		DebuggerInterface::onJitBlockDestroyed(_mode, _pc);

		m_jitState.onJitBlockDestroyed(_mode, _pc);
	}

	void Debugger::addListener(DebuggerListener* _listener)
	{
		m_listeners.insert(_listener);
	}

	void Debugger::removeListener(DebuggerListener* _listener)
	{
		m_listeners.erase(_listener);
	}

	void Debugger::onExec(dsp56k::TWord _addr)
	{
		m_state.setCurrentPC(_addr);

		m_dspExec.onExec(_addr);
	}

	void Debugger::onMemoryWrite(dsp56k::EMemArea _area, dsp56k::TWord _addr, dsp56k::TWord _value)
	{
		m_dspExec.onMemoryWrite(_area, _addr, _value);
	}

	void Debugger::onMemoryRead(dsp56k::EMemArea _area, dsp56k::TWord _addr)
	{
		DebuggerInterface::onMemoryRead(_area, _addr);
	}

	void Debugger::onDebug()
	{
		m_dspExec.onDebug();
	}

	dsp56k::TWord Debugger::disasmAt(dsp56k::Disassembler::Line& _line, const dsp56k::TWord _addr)
	{
		dsp56k::TWord opA, opB;
		dsp().memory().getOpcode(_addr, opA, opB);
		return disasm().disassemble(_line, opA, opB, 0, 0, _addr);
	}

	dsp56k::TWord Debugger::disasmAt(std::string& _dst, dsp56k::TWord _addr, bool _useIndent)
	{
		dsp56k::Disassembler::Line line;
		const auto res = disasmAt(line, _addr);
		_dst = dsp56k::Disassembler::formatLine(line, _useIndent);
		return res;
	}

	dsp56k::TWord Debugger::getBranchTarget(dsp56k::TWord _addr, dsp56k::TWord _pc)
	{
		dsp56k::TWord opA, opB;
		dsp().memory().getOpcode(_addr, opA, opB);
		dsp56k::Instruction instA, instB;
		dsp().opcodes().getInstructionTypes(opA, instA, instB);
		return dsp56k::getBranchTarget(instA, opA, opB, _pc);
	}

	void Debugger::setJitConfig(const dsp56k::JitConfig& _config)
	{
		auto& jit = dsp().getJit();

		dspExec().runOnDspThread([_config, &jit]()
		{
			jit.setConfig(_config);
			return true;
		});
	}

	void Debugger::destroyAllJitBlocks()
	{
		auto& d = dsp();

		dspExec().runOnDspThread([&d]()
		{
			if(d.getSR().toWord() & dsp56k::SR_LF)
				return false;
			if(d.getProcessingMode() != dsp56k::DSP::Default)
				return false;

			d.getJit().destroyAllBlocks();
			return true;
		});
	}

	bool Debugger::getMemoryAddress(dsp56k::TWord& _addr, dsp56k::EMemArea& _area, const dsp56k::TWord& _memAddr)
	{
		dsp56k::TWord opA, opB;
		dsp().memory().getOpcode(_memAddr, opA, opB);
		return dsp().opcodes().getMemoryAddress(_addr, _area, opA, opB);
	}

	void Debugger::windowRunner()
	{
		auto* app = new DebuggerApp(*this);	// deleted by wxWidgets
		wxApp::SetInstance(app);

		int argc=0;
		wxEntry(argc, static_cast<char**>(nullptr));

		wxEntryCleanup();
	}
}
