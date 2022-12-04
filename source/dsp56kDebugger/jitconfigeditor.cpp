#include "jitconfigeditor.h"

#include "debugger.h"
#include "types.h"
#include "wx/propgrid/propgrid.h"

namespace dsp56kDebugger
{
	JitConfigEditor::JitConfigEditor(Debugger& _debugger, wxWindow* _parent) : Dialog(_parent, "JIT Config", wxDefaultPosition, wxSize(600,500)), DebuggerListener(_debugger)
	{
		m_grid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxSize(400,300));

		auto* btApply = new wxButton(this, IdButtonApply, "&Apply && rebuild all JIT blocks");
		auto* btCancel = new wxButton(this, wxID_CANCEL, "&Cancel");
		auto* btOK = new wxButton(this, wxID_OK, "&OK");

		btOK->SetDefault();

		auto* sizerButtons = new wxBoxSizer(wxHORIZONTAL);
		sizerButtons->Add(btApply, 0, wxALL, 10);
		sizerButtons->Add(btCancel, 0, wxTOP | wxBOTTOM, 10);
		sizerButtons->Add(btOK, 0, wxALL, 10);

		auto* sizer = new wxBoxSizer(wxVERTICAL);

		sizer->Add(m_grid, 1, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 10);
		sizer->Add(sizerButtons);

		SetSizerAndFit(sizer);

		refresh();

		btApply->Bind(wxEVT_BUTTON, [&](wxCommandEvent&){ onBtApply();	});
	}

	void JitConfigEditor::refresh()
	{
		m_config = debugger().dsp().getJit().getConfig();

		m_grid->Clear();

		m_propsBool.emplace_back(m_grid, "AGU Support Reverse Carry (bitreverse) addressing", m_config.aguSupportBitreverse);
		m_propsBool.emplace_back(m_grid, "AGU Assume N is positive", m_config.aguAssumePositiveN);
		m_propsBool.emplace_back(m_grid, "AGU Support Multiple-Wrap Modulo", m_config.aguSupportMultipleWrapModulo);
		m_propsBool.emplace_back(m_grid, "Cache Single-Op Blocks", m_config.cacheSingleOpBlocks);
		m_propsBool.emplace_back(m_grid, "Link Blocks", m_config.linkJitBlocks);
		m_propsBool.emplace_back(m_grid, "Split Ops by NOPs", m_config.splitOpsByNops);
		m_propsBool.emplace_back(m_grid, "Support dynamic peripheral addressing", m_config.dynamicPeripheralAddressing);
		m_propsUInt.emplace_back(m_grid, "Max Instructions per Block", m_config.maxInstructionsPerBlock);
		m_propsBool.emplace_back(m_grid, "Do memory writes via C++", m_config.memoryWritesCallCpp);

		for (auto& prop : m_propsBool)
			prop.prop()->SetAttribute(wxPG_BOOL_USE_CHECKBOX, true);
	}

	void JitConfigEditor::onBtApply()
	{
		debugger().setJitConfig(getResult());
		debugger().destroyAllJitBlocks();
		EndModal(wxID_CANCEL);
	}

	const dsp56k::JitConfig& JitConfigEditor::getResult()
	{
		for (auto& prop : m_propsBool)
			prop.read();
		for (auto& prop : m_propsUInt)
			prop.read();

		return m_config;
	}
}
