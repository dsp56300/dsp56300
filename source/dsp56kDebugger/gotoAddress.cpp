#include "gotoAddress.h"

namespace dsp56kDebugger
{
	class Validator : public wxValidator
	{
		wxObject* Clone() const override
		{
			return new Validator(*this);
		}

		bool Validate(wxWindow* w) override
		{
			const auto* tc = dynamic_cast<wxTextCtrl*>(w);
			if(!tc)
				return false;

			std::string t(tc->GetValue());
			if(t.empty())
				return false;
			if(t.front() == '$')
				t.erase(t.begin());
			if(t.size() > 6)
				return false;

			for (const auto c : t)
			{
				if(c >= '0' && c <= '9')	continue;
				if(c >= 'a' && c <= 'f')	continue;
				if(c >= 'A' && c <= 'F')	continue;

				return false;
			}

			return true;
		}

		bool TransferFromWindow() override
		{
			return true;
		}
	};

	Validator g_validator;

	GotoAddress::GotoAddress(wxWindow* _parent/* = nullptr*/) : Dialog(_parent, "Go to Address", wxDefaultPosition, wxSize(500,200))
	{
		auto* sizer = new wxBoxSizer(wxHORIZONTAL);

		m_addr = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER, g_validator);

		m_addr->SetMaxLength(6);

		auto* btCancel = new wxButton(this, wxID_CANCEL, "Cancel");
		auto* btOK = new wxButton(this, wxID_OK, "OK", wxDefaultPosition, wxDefaultSize);
		btOK->SetDefault();

		sizer->Add(m_addr, 0, wxALL, 10);
		sizer->Add(btCancel, 0, wxTOP | wxBOTTOM, 10);
		sizer->Add(btOK, 0, wxALL, 10);

		SetSizerAndFit(sizer);
	}

	dsp56k::TWord GotoAddress::getAddress() const
	{
		std::string t(m_addr->GetValue());

		if(t.front() == '$')
			t.erase(t.begin());

		return static_cast<dsp56k::TWord>(::strtol(t.c_str(), nullptr, 16));
	}
}
