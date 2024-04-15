#include "addressInfo.h"

#include "debugger.h"

#include "dsp56kEmu/opcodeanalysis.h"

namespace dsp56kDebugger
{
	AddressInfo::AddressInfo(Debugger& _debugger, wxWindow* _parent) : wxPropertyGrid(_parent, wxID_ANY, wxDefaultPosition, wxSize(200,200)), DebuggerListener(_debugger)
	{
		SetSplitterPosition(200);
		refresh();
	}

	wxPGProperty* AddressInfo::add(const std::string& _name, const std::string& _value, wxPGProperty* _parent/* = nullptr*/)
	{
		auto* sprop = new wxStringProperty(_name, _name, _value);

		if(_parent)
			return _parent->AppendChild(sprop);
		return Append(sprop);
	}

	std::string toString(dsp56k::JitBlockInfo::TerminationReason _terminationReason)
	{
		switch (_terminationReason)
		{
		case dsp56k::JitBlockInfo::TerminationReason::ExistingCode:		return "Existing Code";
		case dsp56k::JitBlockInfo::TerminationReason::PcMax:			return "Max PC limited";
		case dsp56k::JitBlockInfo::TerminationReason::VolatileP:		return "Volatile P";
		case dsp56k::JitBlockInfo::TerminationReason::WriteLoopRegs:	return "Write LA/LC";
		case dsp56k::JitBlockInfo::TerminationReason::Branch:			return "Branch";
		case dsp56k::JitBlockInfo::TerminationReason::PopPC:			return "Pop PC";
		case dsp56k::JitBlockInfo::TerminationReason::LoopBegin:		return "Loop Begin";
		case dsp56k::JitBlockInfo::TerminationReason::WritePMem:		return "Write P Mem";
		case dsp56k::JitBlockInfo::TerminationReason::LoopEnd:			return "Loop End";
		case dsp56k::JitBlockInfo::TerminationReason::InstructionLimit:	return "Instruction Limit";
		case dsp56k::JitBlockInfo::TerminationReason::ModeChange:		return "Mode Change";
		case dsp56k::JitBlockInfo::TerminationReason::None:				return "None";
		default:														return "?";
		}
	}
	std::string toString(const dsp56k::Register _reg)
	{
		using Reg = dsp56k::Register;

		switch (_reg)
		{
		case Reg::Invalid: return "<invalid>";
		case Reg::A0: return "a0";			case Reg::A1: return "a1";		case Reg::A2: return "a2";
		case Reg::B0: return "b0";			case Reg::B1: return "b1";		case Reg::B2: return "b2";
		case Reg::X0: return "x0";			case Reg::X1: return "x1";
		case Reg::Y0: return "y0";			case Reg::Y1: return "y1";
		case Reg::R0: return "r0";			case Reg::R1: return "r1";		case Reg::R2: return "r2";		case Reg::R3: return "r3";		case Reg::R4: return "r4";		case Reg::R5: return "r5";		case Reg::R6: return "r6";		case Reg::R7: return "r7";
		case Reg::N0: return "n0";			case Reg::N1: return "n1";		case Reg::N2: return "n2";		case Reg::N3: return "n3";		case Reg::N4: return "n4";		case Reg::N5: return "n5";		case Reg::N6: return "n6";		case Reg::N7: return "n7";
		case Reg::M0: return "m0";			case Reg::M1: return "m1";		case Reg::M2: return "m2";		case Reg::M3: return "m3";		case Reg::M4: return "m4";		case Reg::M5: return "m5";		case Reg::M6: return "m6";		case Reg::M7: return "m7";
		case Reg::PC: return "pc";
		case Reg::LA: return "la";			case Reg::LC: return "lc";
		case Reg::SSH: return "ssh";		case Reg::SSL: return "ssl";
		case Reg::SP: return "sp";			case Reg::SC: return "sc";		case Reg::EP: return "ep";
		case Reg::VBA: return "vba";
		case Reg::SZ: return "sz";
		case Reg::EMR: return "emr";		case Reg::MR: return "mr";		case Reg::CCR: return "ccr";
		case Reg::EOM: return "eom";		case Reg::COM: return "com";
		case Reg::A: return "a";			case Reg::B: return "b";
		case Reg::A10: return "a10";		case Reg::B10: return "b10";
		case Reg::AB: return "ab";			case Reg::BA: return "ba";
		case Reg::X: return "x";			case Reg::Y: return "y";
		case Reg::X0X0: return "x0x0";		case Reg::X0X1: return "x0x1";		case Reg::X1X0: return "x1x0";		case Reg::X1X1: return "x1x1";
		case Reg::Y0Y0: return "y0y0";		case Reg::Y0Y1: return "y0y1";		case Reg::Y1Y0: return "y1y0";		case Reg::Y1Y1: return "y1y1";
		case Reg::X0Y0: return "x0y0";		case Reg::X0Y1: return "x0y1";		case Reg::X1Y0: return "x1y0";		case Reg::X1Y1: return "x1y1";
		case Reg::Y0X0: return "y0x0";		case Reg::Y0X1: return "y0x1";		case Reg::Y1X0: return "y1x0";		case Reg::Y1X1: return "y1x1";
		case Reg::SR: return "sr";
		case Reg::OMR: return "omr";
		case Reg::Count: return "<outOfBounds>";
		default: return "?";
		}
	}

	std::string toString(dsp56k::RegisterMask _regs)
	{
		std::string res;

		auto test = [&](const dsp56k::Register _reg)
		{
			const auto m = dsp56k::convert(_reg);
			if((_regs & m) == m)
			{
				if(!res.empty())
					res += " | ";
				res += toString(_reg);
				_regs = static_cast<dsp56k::RegisterMask>(static_cast<uint64_t>(_regs) & ~static_cast<uint64_t>(m));
			}
		};

		test(dsp56k::Register::A);
		test(dsp56k::Register::B);
		test(dsp56k::Register::X);
		test(dsp56k::Register::Y);

		for(uint64_t i=0; i<static_cast<uint64_t>(dsp56k::Register::Count); ++i)
		{
			test(static_cast<dsp56k::Register>(i));
		}

		if(res.empty())
			return "-";
		return res;
	}

	std::string jitBlockFlagsToString(uint32_t _flags)
	{
		std::string res;

		auto addFlag = [&](dsp56k::JitBlockInfo::Flags _flag, const std::string& _name)
		{
			const auto f = static_cast<uint32_t>(_flag);
			if((_flags & f) == 0)
				return;

			_flags &= ~f;

			if(!res.empty())
				res += " | ";
			res += _name;
		};

		addFlag(dsp56k::JitBlockInfo::Flags::IsLoopBodyBegin, "Loop Body Begin");
		addFlag(dsp56k::JitBlockInfo::Flags::ModeChange, "Mode Change");
		addFlag(dsp56k::JitBlockInfo::Flags::WritesSRbeforeRead, "Write SR before read");

		if(_flags)
			addFlag(static_cast<dsp56k::JitBlockInfo::Flags>(_flags), "?" + std::to_string(_flags));

		if(res.empty())
			return "-";
		return res;
	}

	void AddressInfo::refresh()
	{
		refresh(debugger().getState().getFocusedAddress());
	}

	void AddressInfo::refresh(const dsp56k::TWord _addr)
	{
		Clear();

		if(_addr >= debugger().dsp().memory().sizeP())
			return;

		m_addr = _addr;

		add("Space", "P");

		add("Address", addrToString(_addr));

		dsp56k::TWord opA, opB;
		memory().getOpcode(_addr, opA, opB);
		std::string assembly;
		const auto opLen = debugger().disasmAt(assembly, _addr, false);

		std::string content = addrToString(opA);
		if(opLen > 1)
			content += " (, " + addrToString(opB) + ")";
		add( "Content", content);

		add("Assembly", assembly);

		dsp56k::RegisterMask regsWritten = dsp56k::RegisterMask::None, regsRead = dsp56k::RegisterMask::None;

		debugger().dsp().opcodes().getRegisters(regsWritten, regsRead, opA);

		add("Regs read", toString(regsRead));
		add("Regs written", toString(regsWritten));

		const auto* jitBlock = debugger().jitState().getJitBlockInfo(_addr);

		if(jitBlock == nullptr)
		{
			add("JIT Block", "-");
		}
		else
		{
			auto* prop = add("JIT Block", "");

			add("Begin", addrToString(jitBlock->pc), prop);
			add("End", addrToString(jitBlock->pc + jitBlock->memSize - 1), prop);
			add("Memory Size", std::to_string(jitBlock->memSize), prop);
			add("Instructions", std::to_string(jitBlock->instructionCount), prop);
			add("Branch Target", addrToString(jitBlock->branchTarget), prop);
			add("Branch is conditional", jitBlock->branchIsConditional ? "yes" : "no", prop);
			add("Termination Reason", toString(jitBlock->terminationReason), prop);
			add("Flags", jitBlockFlagsToString(jitBlock->flags), prop);
			add("Regs read", toString(jitBlock->readRegs), prop);
			add("Regs written", toString(jitBlock->writtenRegs), prop);
		}
	}

	void AddressInfo::evFocusAddress(dsp56k::TWord _addr)
	{
		DebuggerListener::evFocusAddress(_addr);
		refresh(_addr);
	}

	std::string AddressInfo::addrToString(const dsp56k::TWord _addr)
	{
		if(_addr == dsp56k::g_invalidAddress)
			return "-";
		if(_addr == dsp56k::g_dynamicAddress)
			return "dynamic";
		std::stringstream ss; ss << HEX(_addr);
		return ss.str();
	}
}
