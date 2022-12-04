#include "callstack.h"

#include "addressInfo.h"
#include "debugger.h"

namespace dsp56kDebugger
{
	Callstack::Callstack(Debugger& _debugger, wxWindow* _parent) : Grid(_parent, wxSize(500,300)), DebuggerListener(_debugger)
	{
		SetDefaultCellAlignment(wxALIGN_LEFT, wxALIGN_CENTER);
		SetRowLabelSize(0);

		CreateGrid(8, 5, wxGridSelectNone);

		SetColLabelValue(0, "#");
		SetColLabelValue(1, "PC");
		SetColLabelValue(2, "SR");
		SetColLabelValue(3, "Origin");
		SetColLabelValue(4, "Disasm");

		SetColSize(0, 35);
		SetColSize(4, 320);
	}

	void Callstack::evDspHalt(dsp56k::TWord _addr)
	{
		refresh();
	}

	void Callstack::refresh()
	{
		const auto& dsp = debugger().dsp();
		const auto& reg = dsp.regs();

		int s = dsp.ssIndex();

		if(!s)
			return;

		while(GetNumberRows() > s+1)
			DeleteRows(0);
		while(GetNumberRows() < s+1)
			AppendRows(1, false);

		int row = 0;
		while(s >= 0)
		{
			const auto pc = dsp56k::hiword(reg.ss[s]).toWord();
			const auto sr = dsp56k::loword(reg.ss[s]).toWord();

			SetCellValue(row, 0, std::to_string(s));
			SetCellValue(row, 1, AddressInfo::addrToString(pc));
			SetCellValue(row, 2, AddressInfo::addrToString(sr));

			bool found = false;
			
			std::string op;

			if(pc >= 2)
			{
				for(dsp56k::TWord offset=2; offset>=1; --offset)
				{
					const dsp56k::TWord foundPC = pc - offset;
					const auto len = debugger().disasmAt(op, foundPC, false);
					if(len == offset)
					{
						SetCellValue(row, 3, AddressInfo::addrToString(foundPC));
						SetCellValue(row, 4, op);
						found = true;
						break;
					}
				}
			}

			if(!found)
			{
//				dsp.memory().getOpcode(pc, opA, opB);
//				debugger().disasm().disassemble(op, opA, opB, 0, 0, pc);
				SetCellValue(row, 3, "?");
				SetCellValue(row, 4, "?");
			}

			--s;
			++row;
		}
	}
}
