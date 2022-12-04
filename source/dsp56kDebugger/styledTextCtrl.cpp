#include "styledTextCtrl.h"

#include "dsp56kEmu/logging.h"

namespace dsp56kDebugger
{
	const wxFont g_font(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false);

	StyledTextCtrl::StyledTextCtrl(wxWindow* _parent, wxStandardID _id, const wxPoint& _pos, const wxSize& _size) : wxStyledTextCtrl(_parent, _id, _pos, _size)
	{
		SetFont(g_font);
		StyleSetFont(wxSTC_STYLE_DEFAULT, g_font);
		StyleClearAll();
		SetUseVerticalScrollBar(true);
	}

	bool StyledTextCtrl::isFocused() const
	{
		return m_focused || this->GetSTCFocus();
	}

	void StyledTextCtrl::setLineCount(uint32_t _count)
	{
		m_lines.resize(_count);
	}

	void StyledTextCtrl::replaceLines(int _line, const std::vector<std::string>& _lines)
	{
		int firstDifferent = -1;
		int lastDifferent = -1;

		for (int i=0; i<static_cast<int>(_lines.size()); ++i)
		{
			const auto& line = _lines[i];

			const auto l = _line + i;

			if(line != m_lines[l])
			{
				if(firstDifferent == -1)
					firstDifferent = l;
				lastDifferent = l;
			}

			m_lines[l] = line;
		}

		if(lastDifferent < 0)
			return;

		std::string text;

		for(int l=firstDifferent; l<=lastDifferent; ++l)
		{
			text += _lines[l - _line] + '\n';
			removeAllMarkers(l);
		}

		const auto prevLine = GetCurrentLine();

		const auto count = lastDifferent - firstDifferent + 1;

		const int posStart = firstDifferent > 0 ? GetLineEndPosition(firstDifferent-1) + 1 : 0;
		const int posEnd = GetLineEndPosition(firstDifferent + count - 1) + 1;
		const auto s = GetRange(posStart, posEnd);

//		SetTargetRange(posStart, posEnd);

		Remove(posStart, posEnd);
		InsertText(posStart, text);
		/*
		StartStyling(posStart);
		SetStyling(posEnd - posStart, 1);
		*/
//		ReplaceTarget(text);

//		CallTipShow(posStart, "test\nblar");

		const auto newLine = GetCurrentLine();
		if(newLine != prevLine)
			GotoLine(prevLine);
	}

	void StyledTextCtrl::addMarker(int _line, int _marker)
	{
		const auto it = m_markers.find(_line);

		if(it == m_markers.end())
		{
			std::map<int,int> markers;
			markers.insert(std::make_pair(_marker, MarkerAdd(_line, _marker)));
			m_markers.insert(std::make_pair(_line, markers));
			return;
		}

		if(it->second.find(_marker) == it->second.end())
			it->second.insert(std::make_pair(_marker, MarkerAdd(_line, _marker)));
	}

	void StyledTextCtrl::removeMarker(int _line, int _marker)
	{
		const auto it = m_markers.find(_line);

		if(it == m_markers.end())
			return;

		const auto itMarker = it->second.find(_marker);

		if(itMarker == it->second.end())
			return;

		MarkerDelete(_line, itMarker->first);
		it->second.erase(itMarker);
	}

	void StyledTextCtrl::toggleMarker(int _line, int _marker, bool _enable)
	{
		if(_enable)
			addMarker(_line, _marker);
		else
			removeMarker(_line, _marker);
	}

	void StyledTextCtrl::removeAllMarkers(const int _line)
	{
		const auto it = m_markers.find(_line);

		if(it == m_markers.end())
			return;

		MarkerDelete(_line, -1);

		m_markers.erase(it);
	}

	void StyledTextCtrl::clear()
	{
		Clear();
		m_lines.clear();
		m_markers.clear();
	}

	bool StyledTextCtrl::addBookmark(const int _line)
	{
		const auto s = m_bookmarks.size();
		m_bookmarks.insert(_line);
		return m_bookmarks.size() > s;
	}

	bool StyledTextCtrl::removeBookmark(int _line)
	{
		const auto s = m_bookmarks.size();
		m_bookmarks.erase(_line);
		return m_bookmarks.size() < s;
	}

	void StyledTextCtrl::toggleBookmark(int _line)
	{
		if(!addBookmark(_line))
			removeBookmark(_line);
	}

	bool StyledTextCtrl::gotoNextBookmark()
	{
		if(m_bookmarks.empty())
			return false;

		if(m_bookmarks.size() == 1)
			GotoLine(*m_bookmarks.begin());

		const auto current = GetCurrentLine();

		auto it = std::upper_bound(m_bookmarks.begin(), m_bookmarks.end(), current);

		if(it == m_bookmarks.end())
			it = std::upper_bound(m_bookmarks.begin(), m_bookmarks.end(), 0);

		if(it == m_bookmarks.end())
			return false;

		GotoLine(*it);
		return true;
	}

	bool StyledTextCtrl::hasBookmark(int line) const
	{
		return m_bookmarks.find(line) != m_bookmarks.end();
	}

	void StyledTextCtrl::markerDefineBookmark(const int _markerNumber)
	{
		MarkerDefine(_markerNumber, wxSTC_MARK_BOOKMARK, *wxBLACK, *wxWHITE);				// border, fill
	}

	void StyledTextCtrl::onSetFocus(wxFocusEvent&)
	{
		m_focused = true;
		LOG("Focus Gained");
	}

	void StyledTextCtrl::onLoseFocus(wxFocusEvent&)
	{
		m_focused = false;
		LOG("Focus Lost");
	}

	wxBEGIN_EVENT_TABLE(StyledTextCtrl, wxStyledTextCtrl)
		EVT_SET_FOCUS(StyledTextCtrl::onSetFocus)
		EVT_KILL_FOCUS(StyledTextCtrl::onLoseFocus)
	wxEND_EVENT_TABLE()
}
