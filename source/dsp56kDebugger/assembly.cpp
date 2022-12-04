#include "assembly.h"

#include "debugger.h"
#include "dsp56kEmu/dsp.h"
#include "dsp56kEmu/interrupts.h"
#include "wx/textctrl.h"

namespace dsp56kDebugger
{
	enum MarkerType
	{
		Bookmark,
		Breakpoint,
		JitBlockStart,
		JitBlockMiddle,
		JitBlockEnd,
		PC,
	};

	Assembly::Assembly(Debugger& _debugger, wxWindow* _parent)
		: StyledTextCtrl(_parent, wxID_ANY, wxDefaultPosition, wxSize(900,1000))
		, DebuggerListener(_debugger)
	{
		SetIndent(14);
		SetTabWidth(14);
		StyleSetForeground(2, *wxYELLOW);
		StyleSetBackground(2, wxColour(0, 40, 0));

		StyleSetForeground(1, *wxBLACK);
		StyleSetBackground(1, wxColour(200, 255, 200));
		StyleSetEOLFilled(1, true);
//		StyleSetHotSpot(1, true);

		SetCaretLineVisibleAlways(true);
		SetCaretLineVisible(true);
		SetCaretLineBackground(wxColour(255,230,150));

		const auto grey = wxColor(128,128,128,0);

		MarkerDefine(Breakpoint, wxSTC_MARK_CIRCLE, *wxBLACK, *wxRED);					// border, fill
		markerDefineBookmark(Bookmark);
		MarkerDefine(PC, wxSTC_MARK_SHORTARROW, *wxBLACK, *wxGREEN);					// border, fill
		MarkerDefine(JitBlockStart, wxSTC_MARK_PLUS, grey, *wxBLUE);					// border, fill
		MarkerDefine(JitBlockMiddle, wxSTC_MARK_VLINE, grey, *wxBLUE);					// foreground ignored, bg = fill
		MarkerDefine(JitBlockEnd, wxSTC_MARK_LCORNER, grey, *wxBLUE);					// foreground ignored, bg = fill

		const auto size = _debugger.dsp().memory().size(dsp56k::MemArea_P);

		m_addressToLine.reserve(size);
		m_lineToAddress.reserve(size);

		setLineCount(size);

		std::stringstream ss;

		for(uint32_t i=0; i<size; ++i)
		{
			m_addressToLine.push_back(i);
			m_lineToAddress.push_back(i);
			ss << HEX(i) << ':' << '\n';
		}

		SetText(ss.str());
//		SetReadOnly(true);

		m_invalidCode.resize(size);

		m_refreshTimer.Bind(wxEVT_TIMER, &Assembly::onTimer, this);
		m_refreshTimer.Start(500, false);

		m_calltipTimer.Bind(wxEVT_TIMER, &Assembly::onCallTip, this);
	}

	void Assembly::onChange(wxStyledTextEvent& e)
	{
		const auto addr = static_cast<dsp56k::TWord>(GetCurrentLine());

		debugger().getState().setFocusedAddr(addr);
	}

	void Assembly::onDoubleClick(wxStyledTextEvent& e)
	{
		CallTipShow(e.GetPosition(), "x:foo = $bar");

		e.Skip();

		const auto addr = static_cast<dsp56k::TWord>(e.GetLine());
		auto target = debugger().getBranchTarget(addr, addr);

		if(target == dsp56k::g_invalidAddress || target == dsp56k::g_dynamicAddress)
		{
			dsp56k::EMemArea area;

			if(debugger().getMemoryAddress(target, area, addr) && target < debugger().dsp().memory().size(area))
			{
				LOG("ADDR: " << HEX(target));

				DspExec::runOnUiThread([this, area, target]()
				{
					debugger().getState().setFocusedMemoryAddress(area, target);
				});
			}
			return;
		}

		DspExec::runOnUiThread([this, target]()
		{
			GotoLine(static_cast<int>(target));
		});
	}

	std::string Assembly::formatAsm(const dsp56k::Disassembler::Line& _line)
	{
		std::stringstream ss;
		ss << HEX(_line.pc) << ": " << _line.opName << '\t';

		const auto alu =_line.m_alu.str();
		const auto moveA =_line.m_moveA.str();
		const auto moveB =_line.m_moveB.str();
		const auto comment =_line.m_comment.str();

		ss << alu;
		if(!alu.empty())
			ss << '\t';
		ss << moveA;
		if(!moveB.empty())
			ss << '\t';
		ss << moveB;
		return ss.str();
	}

	void Assembly::disassemble(const int _line, int _count)
	{
		if(_line + _count > static_cast<int>(memory().sizeP()))
			_count = static_cast<int>(memory().sizeP()) - _line;

		std::vector<std::string> newLines;
		newLines.reserve(_count);

		for(auto l=_line; l<_line + _count;)
		{
			dsp56k::TWord opA, opB;
			memory().getOpcode(l, opA, opB);
			dsp56k::Disassembler::Line a;
			int count = std::max(1, static_cast<int>(debugger().disasm().disassemble(a, opA, opB, 0, 0, l)));

			// do not skip interrupts in the case where the second word of an interrupt returned a two-word op
			if((l&1) && count > 1 && l < dsp56k::Vba_End)
				count = 1;

			newLines.push_back(formatAsm(a));

			++l;
			for(int i=1; i<count && l < _line+_count; ++i)
			{
				newLines.emplace_back();
				++l;
			}
		}

		replaceLines(_line, newLines);

		for(auto i=_line; i<_line+_count; ++i)
		{
			const auto* jitInfo = debugger().jitState().getJitBlockInfo(i);

			const auto a = static_cast<dsp56k::TWord>(i);

			toggleMarker(i, JitBlockStart, jitInfo && jitInfo->pc == a);
			toggleMarker(i, JitBlockEnd, jitInfo && a == jitInfo->pc + jitInfo->memSize - 1);
			toggleMarker(i, JitBlockMiddle, jitInfo && a > jitInfo->pc && a < jitInfo->pc + jitInfo->memSize - 1);

			toggleMarker(i, Breakpoint, debugger().getState().hasAddrBreakpoint(i));
			toggleMarker(i, PC, debugger().getState().getCurrentPC() == a);
			toggleMarker(i, Bookmark, hasBookmark(i));
		}

		const auto newLine = GetFirstVisibleLine();
		if(newLine != _line)
			ScrollToLine(_line);
	}

	void Assembly::onTimer(wxTimerEvent&)
	{
		refresh();
	}

	void Assembly::onCallTip(wxTimerEvent&)
	{
		LOG("onCallTip");
	}

	void Assembly::onMotion(wxMouseEvent&)
	{
		if(CallTipActive())
			CallTipCancel();
		m_calltipTimer.Stop();
		m_calltipTimer.Start(500, true);
	}

	void Assembly::evToggleBreakpoint()
	{
		if(isFocused())
			debugger().getState().toggleAddrBreakpoint();
	}

	void Assembly::evBreakpointsChanged()
	{
		refresh();
	}

	void Assembly::evDspHalt(dsp56k::TWord _addr)
	{
//		const auto pos = GetLineEndPosition(_addr);
		GotoLine(_addr);
//		ScrollRange(pos, pos);
	}

	void Assembly::evGotoAddress(dsp56k::TWord _addr)
	{
		if(this->GetSTCFocus())
			GotoLine(static_cast<int>(_addr));
	}

	void Assembly::evGotoPC()
	{
		GotoLine(static_cast<int>(debugger().dsp().regs().pc.toWord()));
	}

	void Assembly::evToggleBookmark()
	{
		if(!isFocused())
			return;
		const auto line = GetCurrentLine();
		toggleBookmark(line);
	}

	void Assembly::evGotoBookmark()
	{
		gotoNextBookmark();
	}

	void Assembly::refresh()
	{
		const auto line = GetFirstVisibleLine();
		constexpr auto range = 100;

		disassemble(line, range);
	}
}

wxBEGIN_EVENT_TABLE(dsp56kDebugger::Assembly, wxStyledTextCtrl)
	EVT_STC_CHANGE(wxID_ANY, Assembly::onChange)
	EVT_STC_PAINTED(wxID_ANY, Assembly::onChange)
	EVT_STC_DOUBLECLICK(wxID_ANY, Assembly::onDoubleClick)
	EVT_MOTION(Assembly::onMotion)
wxEND_EVENT_TABLE() // The button is pressed
