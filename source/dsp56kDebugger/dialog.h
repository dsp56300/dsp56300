#pragma once

#include "wx/wx.h"

namespace dsp56kDebugger
{
	class Dialog : public wxDialog
	{
	public:
		Dialog(wxWindow* _parent, const std::string& _title, const wxPoint& _pos, const wxSize& _size);
	};
}
