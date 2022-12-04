#pragma once

#include <map>
#include <set>

#include "wx/stc/stc.h"

namespace dsp56kDebugger
{
	class StyledTextCtrl : public wxStyledTextCtrl
	{
	public:
		StyledTextCtrl(wxWindow* _parent, wxStandardID _id, const wxPoint& _pos, const wxSize& _size);

		bool isFocused() const;
	protected:
		void setLineCount(uint32_t _count);
		void replaceLines(int _line, const std::vector<std::string>& _lines);
		void addMarker(int _line, int _marker);
		void removeMarker(int _line, int _marker);
		void toggleMarker(int _line, int _marker, bool _enable);
		void removeAllMarkers(int _line);
		void clear();

		bool addBookmark(int _line);
		bool removeBookmark(int _line);
		void toggleBookmark(int _line);
		bool gotoNextBookmark();
		bool hasBookmark(int line) const;
		void markerDefineBookmark(int _markerNumber);

	private:
		void onSetFocus(wxFocusEvent&);
		void onLoseFocus(wxFocusEvent&);

		std::vector<std::string> m_lines;
		std::map<int, std::map<int, int>> m_markers;

		std::set<int> m_bookmarks;

		bool m_focused = false;

		wxDECLARE_EVENT_TABLE();
	};
}
