#include "grid.h"

namespace dsp56kDebugger
{
	const wxFont g_font(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false);

	Grid::Grid(wxWindow* _parent, const wxSize& _size) : wxGrid(_parent, wxID_ANY, wxDefaultPosition, _size)
	{
		SetDefaultCellOverflow(false);
		SetDefaultCellFont(g_font);
		SetDefaultCellFitMode(wxGridFitMode::Clip());
	}
}
