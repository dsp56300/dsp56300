#pragma once

#include "wx/generic/grid.h"

namespace dsp56kDebugger
{
	class Grid : public wxGrid
	{
	public:
		Grid(wxWindow* _parent, const wxSize& _size);
	};
}
