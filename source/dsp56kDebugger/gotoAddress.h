#pragma once
#include "dialog.h"
#include "dsp56kEmu/types.h"

namespace dsp56kDebugger
{
	class GotoAddress : public Dialog
	{
	public:
		GotoAddress(wxWindow* _parent = nullptr);

		dsp56k::TWord getAddress() const;
	private:
		wxTextCtrl* m_addr = nullptr;
	};
}
