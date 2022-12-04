#pragma once

#include "debuggerListener.h"
#include "wx/propgrid/propgrid.h"

namespace dsp56kDebugger
{
	class AddressInfo : public wxPropertyGrid, protected DebuggerListener
	{
	public:
		AddressInfo(Debugger& _debugger, wxWindow* _parent);

		static std::string addrToString(dsp56k::TWord _addr);

	private:
		wxPGProperty* add(const std::string& _name, const std::string& _value, wxPGProperty* _parent = nullptr);
		void refresh();
		void refresh(dsp56k::TWord _addr);
		void evFocusAddress(dsp56k::TWord _addr) override;

		dsp56k::TWord m_addr = 0;
	};
}
