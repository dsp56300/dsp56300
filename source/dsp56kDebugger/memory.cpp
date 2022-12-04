#include "memory.h"

#include "debugger.h"

namespace dsp56kDebugger
{
	Memory::Memory(Debugger& _debugger, wxWindow* _parent, dsp56k::EMemArea _area)
		: StyledTextCtrl(_parent, wxID_ANY, wxDefaultPosition, wxSize(700,600))
		, DebuggerListener(_debugger)
		, m_area(_area)
	{
		markerDefineBookmark(0);

		StyleSetForeground(1, *wxWHITE);
		StyleSetBackground(1, *wxRED);

		SetSelBackground(true, wxColour(255,230,150));

		initialize();

		m_timer.Bind(wxEVT_TIMER, &Memory::onTimer, this);
		m_timer.Start(500, false);
	}

	dsp56k::TWord Memory::addressFromPos(int _pos) const
	{
		const auto line = LineFromPosition(_pos);
		const auto rootAddr = line * m_columnCount;
		const auto rootPos = line > 0 ? GetLineEndPosition(line-1) + 1 : 0;
		const auto offset = _pos - rootPos;

		if(offset < 8)
			return rootAddr;

		return rootAddr + (offset - 8) / 7;
	}

	void Memory::onLeftMouseDown(wxMouseEvent& e)
	{
		SetFocus();

		const auto pos = PositionFromPoint(e.GetPosition());
		const auto addr = addressFromPos(pos);

		debugger().sendEvent(&DebuggerListener::evFocusMemAddress, m_area, addr);
	}

	void Memory::addressToPositions(int& _start, int& _end, dsp56k::TWord _addr) const
	{
		const auto line = static_cast<int>(_addr / m_columnCount);

		const auto rootAddr = line * m_columnCount;

		const auto rootPos = PositionFromLine(line);

		const auto a = _addr - rootAddr;

		_start = static_cast<int>(8 + a * 7 + rootPos);
		_end = _start + 6;
	}

	void Memory::evToggleBreakpoint()
	{
		if(isFocused())
			debugger().getState().toggleMemBreakpoint(m_area);
	}

	void Memory::initialize()
	{
		clear();

		const auto size = memory().size(m_area);

		const auto lineCount = (size + m_columnCount-1) / m_columnCount;

		setLineCount(lineCount);

		std::stringstream ss;
		for(uint32_t i=0; i<lineCount; ++i)
		{
			ss << HEX(i * m_columnCount) << ':' << '\n';
		}

		SetText(ss.str());
	}

	void Memory::refresh()
	{
		const auto line = GetFirstVisibleLine();
		constexpr auto range = 80;

		update(line, range);
	}

	void Memory::onTimer(wxTimerEvent& e)
	{
		refresh();
	}

	void Memory::selectAddress(dsp56k::TWord _addr)
	{
		int start, end;
		addressToPositions(start, end, _addr);
		SetSelection(start, end);
	}

	void Memory::update(int _line, int _count)
	{
		std::vector<std::string> lines;

		for(auto l=_line; l<_line+_count; ++l)
		{
			std::stringstream ss;
			const auto offset = static_cast<dsp56k::TWord>(l) * m_columnCount;

			if(offset >= memory().size(m_area))
				break;

			ss << HEX(offset) << ':';

			for(uint32_t i=0; i<m_columnCount; ++i)
			{
				const auto o = offset + i;
				if(o >= memory().size(m_area))
					break;
				const auto v = memory().get(m_area, o);
				ss << ' ' << HEX(v);
			}
			lines.push_back(ss.str());
		}

		replaceLines(_line, lines);

		selectAddress(m_selectedAddr);

		for(auto i=_line; i<_line + lines.size(); ++i)
		{
			toggleMarker(i, 0, hasBookmark(i));
		}

		const auto posStart = PositionFromLine(_line);
		const auto posEnd = PositionFromLine(_line + _count);
		StartStyling(posStart);
		SetStyling(posEnd - posStart, 0);

		const auto addrFirst = _line * m_columnCount;
		const auto addrLast = (_line + _count) * m_columnCount;

		for(auto i=addrFirst; i<addrLast; ++i)
		{
			if(debugger().getState().hasMemBreakpoint(m_area, i))
			{
				int start, end;
				addressToPositions(start, end, i);
				StartStyling(start);
				SetStyling(end - start, 1);
			}
		}
	}

	void Memory::evGotoAddress(dsp56k::TWord _addr)
	{
		if(isFocused())
		{
			const auto line = _addr / m_columnCount;
			GotoLine(static_cast<int>(line));
		}
	}

	void Memory::evToggleBookmark()
	{
		if(!isFocused())
			return;
		const auto line = GetCurrentLine();
		toggleBookmark(line);
	}

	void Memory::evGotoBookmark()
	{
		gotoNextBookmark();
	}

	void Memory::evFocusMemAddress(dsp56k::EMemArea _area, dsp56k::TWord _addr)
	{
		if(_area != m_area)
			return;
		m_selectedAddr = _addr;
		selectAddress(m_selectedAddr);
		GotoLine(m_selectedAddr / m_columnCount);
	}

	void Memory::evMemBreakpointsChanged(dsp56k::EMemArea _area)
	{
		refresh();
	}

	wxBEGIN_EVENT_TABLE(Memory, wxStyledTextCtrl)
		EVT_LEFT_DOWN(Memory::onLeftMouseDown)
	wxEND_EVENT_TABLE()
}
