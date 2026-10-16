#pragma once

#include <cstdint>

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

		// True if this is the StyledTextCtrl that most recently had the
		// keyboard focus among all StyledTextCtrl instances, even if none
		// of them currently has focus (e.g. because a modal dialog such as
		// "Go to Address" is/was open, or briefly took focus in between).
		// Use this instead of isFocused()/GetSTCFocus() when handling
		// commands that are triggered right after such a dialog closes,
		// since at that point the "real" focus may not have been restored
		// yet (this is timing-/platform-dependent).
		bool isLastFocused() const;

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

		// Registers this control as the default "last focused" target
		// (see isLastFocused()) to be used as long as no StyledTextCtrl has
		// actually received keyboard focus yet, i.e. before the very first
		// focus change. Has no effect once any StyledTextCtrl has been
		// focused. Intended to be called once from a derived class'
		// constructor.
		void setAsDefaultFocus();

	private:
		void onSetFocus(wxFocusEvent&);
		void onLoseFocus(wxFocusEvent&);

		std::vector<std::string> m_lines;
		std::map<int, std::map<int, int>> m_markers;

		std::set<int> m_bookmarks;

		bool m_focused = false;

		static StyledTextCtrl* s_lastFocused;

		wxDECLARE_EVENT_TABLE();
	};
}
