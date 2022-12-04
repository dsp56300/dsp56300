#pragma once

#include "debuggerListener.h"
#include "dialog.h"
#include "wx/propgrid/propgrid.h"

namespace dsp56kDebugger
{
	class JitConfigEditor : public Dialog, protected DebuggerListener
	{
	public:
		JitConfigEditor(Debugger& _debugger, wxWindow* _parent);

		const dsp56k::JitConfig& getResult();

	private:
		template<typename TProperty, typename TValue>
		struct PropertyRef
		{
			PropertyRef(wxPropertyGrid* _grid, const std::string& _label, TValue& _value)
				: m_prop(new TProperty(_label, wxString(), _value)), m_value(_value)
			{
				_grid->Append(m_prop);
			}

			void read() const
			{
				read(m_value);
			}

			TProperty* prop()
			{
				return m_prop;
			}
			bool& value()
			{
				return m_value;
			}
		private:
			void read(bool& _value) const
			{
				_value = m_prop->GetValue();
			}
			void read(uint32_t& _value) const
			{
				long v = m_prop->GetValue();
				_value = static_cast<uint32_t>(v);
			}
			TProperty* m_prop;
			TValue& m_value;
		};

		void refresh();
		void onBtApply();

		wxPropertyGrid* m_grid = nullptr;
		dsp56k::JitConfig m_config;
		std::vector<PropertyRef<wxBoolProperty, bool>> m_propsBool;
		std::vector<PropertyRef<wxUIntProperty, uint32_t>> m_propsUInt;
		
	};
}
