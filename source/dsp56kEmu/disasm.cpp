#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

#include "disasm.h"

#include <algorithm>

#include "aar.h"
#include "interrupts.h"
#include "logging.h"
#include "memory.h"
#include "peripherals.h"
#include "registers.h"
#include "utils.h"
#include "opcodeanalysis.h"

#ifdef USE_MOTOROLA_UNASM
extern "C"
{
	#include "../dsp56k/PROTO563.H"
}
#endif

namespace dsp56k
{
	constexpr const char* g_opNames[] = 
	{
		"abs",
		"adc",
		"add",		"add",		"add",
		"addl",		"addr",
		"and",		"and",		"and",		"andi",
		"asl",		"asl",		"asl",
		"asr",		"asr",		"asr",
		"b",		"b",		"b",
		"bchg",		"bchg",		"bchg",		"bchg",		"bchg",
		"bclr",		"bclr",		"bclr",		"bclr",		"bclr",
		"bra",		"bra",		"bra",
		"brclr",	"brclr",	"brclr",	"brclr",	"brclr",
		"brk",
		"brset",	"brset",	"brset",	"brset",	"brset",
		"bs",		"bs",		"bs",
		"bsclr",	"bsclr",	"bsclr",	"bsclr",	"bsclr",
		"bset",		"bset",		"bset",		"bset",		"bset",
		"bsr",		"bsr",		"bsr",
		"bsset",	"bsset",	"bsset",	"bsset",	"bsset",
		"btst",		"btst",		"btst",		"btst",		"btst",
		"clb",
		"clr",
		"cmp",		"cmp",		"cmp",
		"cmpm",		"cmpu",
		"debug",	"debug",
		"dec",
		"div",
		"dmac",
		"do",		"do",		"do",		"do",
		"do forever,",
		"dor",		"dor",		"dor",		"dor",
		"dor forever,",
		"enddo",
		"eor",		"eor",		"eor",
		"extract",	"extract",
		"extractu",	"extractu",
		"",			"",																						// ifcc
		"illegal",	"inc",
		"insert",	"insert",
		"j",		"j",
		"jclr",		"jclr",		"jclr",		"jclr",		"jclr",
		"jmp",		"jmp",
		"js",		"js",
		"jsclr",	"jsclr",	"jsclr",	"jsclr",	"jsclr",
		"jset",		"jset",		"jset",		"jset",		"jset",
		"jsr",		"jsr",
		"jsset",	"jsset",	"jsset",	"jsset",	"jsset",
		"lra",		"lra",
		"lsl",		"lsl",		"lsl",
		"lsr",		"lsr",		"lsr",
		"lua",		"lua",
		"mac",		"mac",
		"maci",
		"mac",
		"macr",	"macr",
		"macri",
		"max",
		"maxm",
		"merge",
		"move",		"move",																				// movenop, move_xx
		"move",																							// move_xx
		"move",																							// move_ea
		"move",		"move",		"move",		"move",														// movex
		"move",		"move",																				// movexr
		"move",		"move",		"move",		"move",														// movey
		"move",		"move",																				// moveyr
		"move",		"move",																				// movel
		"move",																							// movexy
		"move",		"move",		"move",		"move",														// movec
		"move",		"move",																				// movem
		"movep",	"movep",	"movep",	"movep",	"movep",	"movep",	"movep",	"movep",	// movep
		"mpy",		"mpy",		"mpy",
		"mpyi",
		"mpyr",		"mpyr",
		"mpyri",
		"neg",
		"nop",
		"norm",
		"normf",
		"not",
		"or",		"or",		"or",
		"ori",
		"pflush",
		"pflushun",
		"pfree",
		"plock",
		"plockr",
		"punlock",
		"punlockr",
		"rep",		"rep",		"rep",		"rep",
		"reset",
		"rnd",
		"rol",
		"ror",
		"rti",
		"rts",
		"sbc",
		"stop",
		"sub",		"sub",		"sub",
		"subl",
		"subr",
		"t",		"t",		"t",
		"tfr",
		"trap",
		"trap",
		"tst",
		"vsl",
		"wait",
		"resolvecache",
		"parallel",
	};

	static_assert(std::size(g_opNames) == InstructionCount, "operation names entries missing or too many");

	const char* g_conditionCodes[16] =	{"cc", "ge", "ne", "pl", "nn", "ec", "lc", "gt", "cs", "lt", "eq", "mi", "nr", "es", "ls", "le"};

	constexpr auto g_memArea_L = MemArea_COUNT;

	constexpr uint32_t columnOp = 2;
	constexpr uint32_t columnA = columnOp + 6;
	constexpr uint32_t columnB = columnA + 16;
	constexpr uint32_t columnC = columnB + 16;
	constexpr uint32_t columnComment = 70;

	std::string hex(TWord _data)
	{
		std::stringstream ss; ss << '$' << std::hex << _data; return std::string(ss.str());
	}

	const char* memArea(EMemArea _area)
	{
		switch (_area)
		{
		case MemArea_X: return "x:";
		case MemArea_Y: return "y:";
		case MemArea_P: return "p:";
		default:		return "?:";
		}
	}

	std::string Disassembler::immediate(const char* _prefix, TWord _data) const
	{
		std::string sym;
		if(getSymbol(Immediate, _data, sym))
			return std::string(_prefix) + sym;

		return std::string(_prefix) + hex(_data);
	}

	std::string Disassembler::bit(TWord _bit, EMemArea _area, TWord _referencingAddress) const
	{
		if(_area == MemArea_COUNT || _referencingAddress == g_invalidAddress)
			return immediate(_bit);
		const auto type = getSymbolType(_area);
		if(type == SymbolTypeCount)
			return immediate(_bit);
		const auto& symbols = m_bitSymbols[type];
		const auto it = symbols.find(_referencingAddress);
		if(it == symbols.end())
			return immediate(_bit);
		const auto& bs = it->second;
		const auto itBit = bs.bits.find(1<<_bit);
		if(itBit == bs.bits.end())
			return immediate(_bit);
		return "#" + itBit->second;
	}

	std::string Disassembler::immediate(TWord _data) const
	{
		return immediate("#", _data);
	}

	std::string Disassembler::immediateShort(TWord _data) const
	{
		return immediate("#<", _data);
	}

	std::string Disassembler::immediateLong(TWord _data) const
	{
		return immediate("#>", _data);
	}

	std::string Disassembler::relativeAddr(EMemArea _area, int _data, bool _long) const
	{
		std::string sym;
		if(getSymbol(_area, m_pc + _data, sym))
			return sym;

		const auto a = signextend<int,24>(_data);
		char temp[3] = {'>', '*', 0};
		temp[0] = _long ? '<' : '>';

		if(_data == 0)
			return temp;
		
		return std::string(temp) + (a >= 0 ? "+" : "-") + hex(std::abs(_data));
	}

	std::string Disassembler::relativeLongAddr(EMemArea _area, int _data) const
	{
		return relativeAddr(_area, _data, true);
	}

	std::string Disassembler::absAddr(EMemArea _area, int _data, bool _long, bool _withArea) const
	{
		std::string res;

		if(_withArea)
			res = std::string(memArea(_area));

		if(_long)
			res += '>';

		std::string sym;
		if(getSymbol(_area, _data, sym))
			return res + sym;
		return res + hex(_data);
	}

	bool Disassembler::functionName(std::string& _result, const TWord _addr) const
	{
		if(getSymbol(MemP, _addr, _result))
			return true;
		std::stringstream ss;
		if(_addr < Vba_End)
			ss << "int_" << HEX(_addr);
		else
			ss << "func_" << HEX(_addr);
		_result = ss.str();
		return false;
	}

	std::string Disassembler::funcAbs(TWord _addr) const
	{
		std::string res;
		functionName(res, _addr);
		return res;
	}

	std::string Disassembler::funcRel(TWord _addr) const
	{
		const int offset = signextend<int, 24>(static_cast<int>(_addr));
		return funcAbs(m_pc + offset);
	}

	std::string Disassembler::absShortAddr(EMemArea _area, int _data) const
	{
		std::string sym;
		if(getSymbol(_area, _data, sym))
			return "<" + sym;

		return "<" + hex(_data);
	}

	const char* aluD(bool ab)
	{
		return ab ? "b" : "a";
	}

	Disassembler::Disassembler(const Opcodes& _opcodes) : m_opcodes(_opcodes)
	{
	}

	std::string Disassembler::formatLine(const Line& _line, bool _useColumnWidths/* = true*/)
	{
		const auto alu = _line.m_alu.str();
		const auto moveA = _line.m_moveA.str();
		const auto moveB = _line.m_moveB.str();
//		const auto comment = _line.m_comment.str();

		std::string res = _line.opName;

		auto makeColumn = [&](uint32_t _column, const std::string& _append)
		{
			if(_append.empty())
				return;

			res += ' ';

			if(_useColumnWidths)
			{
				while(res.size() < _column)
					res += ' ';
			}

			res += _append;
		};

		if(_useColumnWidths)
		{
			for(size_t i=0; i<columnOp; ++i)
				res += ' ';
		}

		makeColumn(columnA, alu);
		makeColumn(alu.empty() ? columnA : columnB, moveA);
		makeColumn(alu.empty() ? columnB : columnC, moveB);
//		makeColumn(columnComment, comment);
		return res;
	}

	void Disassembler::disassembleDC(Line& dst, TWord op)
	{
		dst.opName = "dc";
		std::stringstream ss;
		dst.m_alu << '$' << HEX(op);
	}

	bool Disassembler::disassembleMemoryBlock(std::string& dst, const std::vector<uint32_t>& _memory, TWord _pc, bool _skipNops, bool _writePC, bool _writeOpcodes)
	{
		if (_memory.empty())
			return false;

		std::stringstream out;

		std::set<TWord> branchTargets;
		std::map<TWord, std::set<TWord>> branchSources;

		std::vector<Line> lines;

		for(auto i = (_pc & 0xfffffe); i<Vba_End; i += 2)
			branchTargets.insert(i);

		for (TWord i = 0; i < _memory.size();)
		{
			const TWord a = _memory[i];
			const TWord b = i + 1 < _memory.size() ? _memory[i + 1] : 0;

			if (_skipNops && !a)
			{
				++i;
				continue;
			}

			Line assem;
			const auto len = disassemble(assem, a, b, 0, 0, _pc + i);

			if(len)
			{
				const auto branchTarget = getBranchTarget(assem.instA, a, b, _pc + i);

				if(branchTarget != g_invalidAddress && branchTarget != g_dynamicAddress)
				{
					if(branchTarget >= Vba_End || ((branchTarget&1) == 0))
					{
						branchTargets.insert(branchTarget);
						branchSources[branchTarget].insert(_pc + i);
					}
				}
			}

			if (!len)
				++i;
			else
				i += len;

			lines.emplace_back(std::move(assem));
		}

		for (const auto & line : lines)
		{
			std::stringstream lineOut;

			if(_writePC)
				lineOut << HEX(line.pc) << ": ";

			if(branchTargets.find(line.pc) != branchTargets.end())
			{
				out << std::endl;
				std::stringstream funcName;
				funcName << funcAbs(line.pc) << ":";

				const auto& callers = branchSources[line.pc];

				if(!callers.empty())
				{
					while(funcName.tellp() < columnComment)
						funcName << ' ';
					funcName << "; (callers:";

					for (const auto& caller : callers)
					{
						std::string f;
						if(functionName(f, caller))
							funcName << ' ' << f;
						else
							funcName << ' ' << "$" << HEX(caller);
					}
					funcName << ')';
				}
				out << funcName.str() << std::endl;
			}

			lineOut << formatLine(line);

			const auto comment = line.m_comment.str();

			if(_writeOpcodes || !comment.empty())
			{
				while(lineOut.tellp() < columnComment)
					lineOut << ' ';

				lineOut << "; ";

				if(!comment.empty())
					lineOut << comment;

				if(_writeOpcodes)
				{
					if(!comment.empty())
						lineOut << ' ';

					lineOut << HEX(line.opA);

					if(line.len == 2)
						lineOut << ' ' << HEX(line.opB);
				}
			}

			out << std::string(lineOut.str()) << std::endl;			
		}

		dst = std::string(out.str());
		return true;
	}

	const char* aluJJJ(TWord JJJ, bool ab)
	{
		switch (JJJ)
		{
			case 0://	return "?";
			case 1:		return aluD(!ab);	// ab = D, if D is a, the result is b and if D is b, the result is a
			case 2:		return "x";
			case 3:		return "y";
			case 4:		return "x0";
			case 5:		return "y0";
			case 6:		return "x1";
			case 7:		return "y1";
			default:	return "?";
		}
	}

	const char* aluQQQ(TWord QQQ)
	{
		switch (QQQ)
		{
		case 0:	return "x0,x0";
		case 1:	return "y0,y0";
		case 2:	return "x1,x0";
		case 3:	return "y1,y0";
		case 4:	return "x0,y1";
		case 5:	return "y0,x0";
		case 6:	return "x1,y0";
		case 7:	return "y1,x1";
		default: return nullptr;
		}
	}

	EMemArea memArea(TWord _area)
	{
		if(_area > 1)
			return MemArea_P;
		return _area ? MemArea_Y : MemArea_X;
	}

	std::string Disassembler::peripheral(EMemArea area, TWord a, TWord _root) const
	{
		std::string sym;
		if(getSymbol(area, _root + a, sym))
			return std::string(memArea(area)) + "<<" + sym;

		assert((_root + a) <= 0xffffff);

		return std::string(memArea(area)) + "<<" + hex(_root + a);
	}

	std::string Disassembler::peripheralQ(EMemArea S, TWord a) const
	{
		return peripheral(S, a, 0xffff80);
	}

	std::string Disassembler::peripheralP(EMemArea S, TWord a) const
	{
		return peripheral(S, a, 0xffffc0);
	}

	TWord Disassembler::peripheralPaddr(const TWord p)
	{
		return p + 0xffffc0;
	}

	TWord Disassembler::peripheralQaddr(const TWord q)
	{
		return q + 0xffff80;
	}

	std::string mmrrr(TWord mmmrrr)
	{
		const char* formats[4]
		{
			"(r%d)",
			"(r%d)+n%d",
			"(r%d)-",
			"(r%d)+"
		};

		char temp[32];
	
		const auto mmm = (mmmrrr>>3)&3;
		const auto r = mmmrrr & 7;
		const auto* format = formats[mmm];
		if(!format)
			return "?";
		sprintf(temp, format, r, r);
		return temp;
	}

	std::string Disassembler::mmmrrr(TWord _mmmrrr, TWord S, TWord opB, bool _addMemSpace, bool _long, bool _isJump)
	{
		return mmmrrr(_mmmrrr, memArea(S), opB, _addMemSpace, _long, _isJump);
	}

	std::string Disassembler::mmmrrr(TWord mmmrrr, EMemArea S, TWord opB, bool _addMemSpace, bool _long, bool _isJump)
	{
		const char* formats[8]
		{
			"(r%d)-n%d",
			"(r%d)+n%d",
			"(r%d)-",
			"(r%d)+",
			"(r%d)",			
			"(r%d+n%d)",
			nullptr,
			"-(r%d)",
		};

		const char* ar = _addMemSpace ? memArea(S) : "";
		const std::string area(ar);

		char temp[32];
	
		switch (mmmrrr)
		{
		case MMMRRR_AbsAddr:
			++m_extWordUsed;
			if(S == MemArea_P && _isJump)
				return funcAbs(opB);
			if(opB >= XIO_Reserved_High_First && S != g_memArea_L)
				return peripheral(S, opB, 0);
			return absAddr(S, opB, _long, true);
		case MMMRRR_ImmediateData:
			++m_extWordUsed;
			return immediateLong(opB);
		default:
			{
				const auto mmm = (mmmrrr>>3)&7;
				const auto r = mmmrrr & 7;
				const auto* format = formats[mmm];
				if(!format)
					return {};
				sprintf(temp, format, r, r);
			}
			return area + temp;
		}
	}

	TWord Disassembler::mmmrrrAddr(TWord mmmrrr, TWord opB)
	{
		switch (mmmrrr)
		{
		case MMMRRR_AbsAddr:
		case MMMRRR_ImmediateData:
			return opB;
		}
		return g_invalidAddress;
	}

	const char* condition(const TWord _cccc)
	{
		return g_conditionCodes[_cccc];
	}

	const char* decodeEE(TWord _ee)
	{
		switch (_ee)
		{
		case 0:	return "mr";
		case 1:	return "ccr";
		case 2:	return "omr";	// it is "com" but Motorola displays it as omr as it is only used in opcodes that work with 8 bit data
		case 3:	return "eom";
		default: return "?";
		}
	}

	
	const char* decode_sss( TWord _sss )
	{
		switch( _sss )
		{
		case 2:		return "a1";
		case 3:		return "b1";
		case 4:		return "x0";
		case 5:		return "y0";
		case 6:		return "x1";
		case 7:		return "y1";
		default:	return "?";
		}
	}
	
	const char* decode_qqq( TWord _sss )
	{
		switch( _sss )
		{
		case 2:		return "a0";
		case 3:		return "b0";
		case 4:		return "x0";
		case 5:		return "y0";
		case 6:		return "x1";
		case 7:		return "y1";
		default:	return "?";
		}
	}

	const char* decode_alu_GGG( TWord _ggg, bool D )
	{
		switch( _ggg )
		{
		case 0:		return aluD(!D);
		case 1:		return "?";
		case 2:		return "?";
		case 3:		return "?";
		case 4:		return "x0";
		case 5:		return "y0";
		case 6:		return "x1";
		case 7:		return "y1";
		default:	return "?";
		}
	}
	
	std::string decode_RRR(TWord _r)
	{
		char temp[3] = "r0";
		temp[1] = '0' + _r;
		return temp;
	}

	std::string decode_RRR( TWord _r, TWord shortDisplacement)
	{
		const auto displacement = signextend<int,24>(shortDisplacement);
		char temp[16] = "(r0)";
		if(displacement >= 0)
			sprintf(temp, "(r%d+%s)", _r, hex(displacement).c_str());
		else
			sprintf(temp, "(r%d-%s)", _r, hex(-displacement).c_str());
		return temp;
	}

	std::string decode_DDDDDD( TWord _ddddd )
	{
		switch( _ddddd )
		{
		case 0x00:
		case 0x04:	return "x0";
		case 0x01:
		case 0x05:	return "x1";
		case 0x02:
		case 0x06:	return "y0";
		case 0x03:
		case 0x07:	return "y1";
		case 0x08:	return "a0";
		case 0x09:	return "b0";
		case 0x0a:	return "a2";
		case 0x0b:	return "b2";
		case 0x0c:	return "a1";
		case 0x0d:	return "b1";
		case 0x0e:	return "a";	
		case 0x0f:	return "b";	
		}

		char temp[3]{0,0,0};
		if( (_ddddd & 0x18) == 0x10 )					// r0-r7
			temp[0] = 'r';
		else if( (_ddddd & 0x18) == 0x18 )				// n0-n7
			temp[0] = 'n';
		else
			return {};

		temp[1] = '0' + (_ddddd&0x07);

		return temp;
	}

	const char* decode_dddddd( TWord _dddddd )
	{
		switch( _dddddd & 0x3f )
		{
			// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:	return "x0";
		case 0x01:	return "x1";
		case 0x02:	return "y0";
		case 0x03:	return "y1";
			// 0001DD - 4 registers in data ALU
		case 0x04:	return "x0";
		case 0x05:	return "x1";
		case 0x06:	return "y0";
		case 0x07:	return "y1";

			// 001DDD - 8 accumulators in data ALU
		case 0x08:	return "a0";
		case 0x09:	return "b0";
		case 0x0a:	return "a2";
		case 0x0b:	return "b2";
		case 0x0c:	return "a1";
		case 0x0d:	return "b1";
		case 0x0e:	return "a";
		case 0x0f:	return "b";

			// 010TTT - 8 address registers in AGU
		case 0x10:	return "r0";
		case 0x11:	return "r1";
		case 0x12:	return "r2";
		case 0x13:	return "r3";
		case 0x14:	return "r4";
		case 0x15:	return "r5";
		case 0x16:	return "r6";
		case 0x17:	return "r7";

			// 011NNN - 8 address offset registers in AGU
		case 0x18:	return "n0";
		case 0x19:	return "n1";
		case 0x1a:	return "n2";
		case 0x1b:	return "n3";
		case 0x1c:	return "n4";
		case 0x1d:	return "n5";
		case 0x1e:	return "n6";
		case 0x1f:	return "n7";

			// 100FFF - 8 address modifier registers in AGU
		case 0x20:	return "m0";
		case 0x21:	return "m1";
		case 0x22:	return "m2";
		case 0x23:	return "m3";
		case 0x24:	return "m4";
		case 0x25:	return "m5";
		case 0x26:	return "m6";
		case 0x27:	return "m7";

			// 101EEE - 1 adress register in AGU
		case 0x2a:	return "ep";

			// 110VVV - 2 program controller registers
		case 0x30:	return "vba";
		case 0x31:	return "sc";

			// 111GGG - 8 program controller registers
		case 0x38:	return "sz";
		case 0x39:	return "sr";
		case 0x3a:	return "omr";
		case 0x3b:	return "sp";
		case 0x3c:	return "ssh";
		case 0x3d:	return "ssl";
		case 0x3e:	return "la";
		case 0x3f:	return "lc";
		default:	return "?";
		}
	}

	std::string decode_ddddd_pcr( TWord _ddddd )
	{
		if( (_ddddd & 0x18) == 0x00 )
		{
			return std::string("m") + static_cast<char>('0' + (_ddddd & 0x07));
		}

		switch( _ddddd )
		{
		case 0xa:	return "ep";
		case 0x10:	return "vba";
		case 0x11:	return "sc";
		case 0x18:	return "sz";
		case 0x19:	return "sr";
		case 0x1a:	return "omr";
		case 0x1b:	return "sp";
		case 0x1c:	return "ssh";
		case 0x1d:	return "ssl";
		case 0x1e:	return "la";
		case 0x1f:	return "lc";
		default:	return "?";
		}
	}

	const char* decode_LLL( TWord _lll)
	{
		switch( _lll )
		{
		case 0:		return "a10";
		case 1:		return "b10";
		case 2:		return "x";
		case 3:		return "y";
		case 4:		return "a";
		case 5:		return "b";
		case 6:		return "ab";
		case 7:		return "ba";
		default:	return "?,?";
		}
	}
	
	uint32_t Disassembler::disassembleAlu(std::stringstream& _dst, Instruction inst, dsp56k::TWord op)
	{
		const auto D = (op>>3) & 0x1;

		if(op>>7 == 0)
		{
			// non-multiply
			const auto kkk = op & 0x7;
			const auto JJJ = (op>>4) & 0x7;

			if(!kkk && !JJJ)
				return 0;

			switch (inst)
			{
			case Tst:
			case Rnd:
			case Clr:
			case Not:
			case Neg:
			case Asr_D:
			case Lsr_D:
			case Asl_D:
			case Lsl_D:
			case Abs:
			case Ror:
			case Rol:
				_dst << aluD(D);
				break;
			case Max:
			case Maxm:
				_dst << "a,b";
				break;
			default:
				const char* j = aluJJJ(JJJ, D);

				if(!j)
				{
					_dst << aluD(D);
				}
				else
				{
					_dst << j << "," << aluD(D);
				}
			}

			return 1;
		}

		// multiply
		const auto QQQ = (op>>4) & 0x7;
		const char* q = aluQQQ(QQQ);
		if(!q)
			return 0;

		const auto negative = (op>>2) & 0x1;

		if(negative)
			_dst << '-';

		_dst << q << "," << aluD(D);

		return 1;
	}

	const char* decode_JJ_read( TWord jj )
	{
		switch( jj )
		{
		case 0:		return "x0";
		case 1:		return "y0";
		case 2:		return "x1";
		case 3:		return "y1";
		default:	return "?";
		}
		
	}

	const char* decode_QQQQ( TWord _qqqq )
	{
		switch( _qqqq )
		{
		case 0:  return "x0,x0";
		case 1:  return "y0,y0";
		case 2:  return "x1,x0";
		case 3:  return "y1,y0";
		case 4:  return "x0,y1";
		case 5:  return "y0,x0";
		case 6:  return "x1,y0";
		case 7:  return "y1,x1";
		case 8:  return "x1,x1";
		case 9:  return "y1,y1";
		case 10: return "x0,x1";
		case 11: return "y0,y1";
		case 12: return "y1,x0";
		case 13: return "x0,y0";
		case 14: return "y0,x1";
		case 15: return "x1,y1";
		default: return "?,?";
		}
	}

	const char* decode_QQ_read( TWord _qq )
	{
		switch( _qq )
		{
		case 0:		return "y1";
		case 1:		return "x0";
		case 2:		return "y0";
		case 3:		return "x1";
		default:	return "?";
		}
	}

	const char* decode_qq_read( TWord _qq )
	{
		switch( _qq )
		{
		case 0:		return "x0";
		case 1:		return "y0";
		case 2:		return "x1";
		case 3:		return "y1";
		default:	return "?";
		}
	}

	const char* decode_ee( TWord _ee )
	{
		switch( _ee )
		{
		case 0:	return "x0";
		case 1:	return "x1";
		case 2:	return "a";
		case 3:	return "b";
		default: return "?";
		}
	}

	const char* decode_ff( TWord _ff)
	{
		switch( _ff )
		{
		case 0:	return "y0";
		case 1:	return "y1";
		case 2:	return "a";
		case 3:	return "b";
		default: return "?";
		}
	}

	std::string decode_TTT(TWord _ttt)
	{
		char temp[3] = {'r', 0, 0};
		temp[1] = '0' + static_cast<char>(_ttt);
		return temp;
	}

	uint32_t Disassembler::disassemble(std::stringstream& _dst, const OpcodeInfo& oi, TWord op, TWord opB)
	{
		if(m_pc == 0x000d40)
			int foo=0;

		const auto inst = oi.m_instruction;

		// switch-case of death incoming in 3..2..1...

		switch (inst)
		{
		// ALU operations without any other variables
		case Dec:
		case Inc:
			{
				const auto d = getFieldValue(inst, Field_d, op);				
				_dst << aluD(d);
			}
			return 1;			
		// immediate operand with destination ALU register
		case Add_xx:
		case And_xx:
		case Cmp_xxS2:
		case Eor_xx:
		case Or_xx:
		case Sub_xx:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto i = getFieldValue(inst, Field_iiiiii, op);
				_dst << immediateShort(i) << ',' << aluD(d);
				return 1;
			}
		// immediate extension operand with destination ALU register
		case Add_xxxx:
		case And_xxxx:
		case Cmp_xxxxS2:
		case Eor_xxxx:
		case Or_xxxx:
		case Sub_xxxx:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				_dst << immediateLong(opB) << ',' << aluD(d);
				return 2;
			}
		// bit manipulation on effective address
		case Bchg_ea:
		case Bclr_ea:
		case Bset_ea:
		case Btst_ea:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), mmmrrrAddr(mr, opB)) << ',' << mmmrrr(mr, S, opB);
				return 1 + m_extWordUsed;
			}
		case Bchg_aa:
		case Bclr_aa:
		case Bset_aa:
		case Btst_aa:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), a) << ',' << absAddr(memArea(S), a);
				return 1;
			}
		case Bchg_pp:
		case Bclr_pp:
		case Bset_pp:
		case Btst_pp:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto p	= getFieldValue(inst, Field_pppppp, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), peripheralPaddr(p)) << ',' << peripheralP(memArea(S), p);
				return 1;
			}
		case Bchg_qq:
		case Bclr_qq:
		case Bset_qq:
		case Btst_qq:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto q	= getFieldValue(inst, Field_qqqqqq, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), peripheralQaddr(q)) << ',' << peripheralQ(memArea(S), q);
				return 1;
			}
		case Bchg_D:
		case Bclr_D:
		case Bset_D:
		case Btst_D:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				_dst << bit(b, MemArea_COUNT, g_invalidAddress) << ',' << decode_dddddd(d);
				return 1;
			}
		// Branch if bit condition is met
		case Brclr_ea:
		case Brset_ea:
		case Bsclr_ea:
		case Bsset_ea:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), mmmrrrAddr(mr, opB)) << ',' << mmmrrr(mr, S, opB) << ',' << funcRel(opB);
				return 2;
			}
		case Brclr_aa:
		case Brset_aa:
		case Bsclr_aa:
		case Bsset_aa:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), a) << ',' << absAddr(memArea(S), a) << ',' << funcRel(opB);
				return 2;
			}
		case Brclr_pp:
		case Brset_pp:
		case Bsclr_pp:
		case Bsset_pp:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto p	= getFieldValue(inst, Field_pppppp, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), peripheralPaddr(p)) << ',' << peripheralP(memArea(S), p) << ',' << funcRel(opB);
				return 2;
			}
		case Brclr_qq:
		case Brset_qq:
		case Bsclr_qq:
		case Bsset_qq:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto q	= getFieldValue(inst, Field_qqqqqq, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), peripheralQaddr(q)) << ',' << peripheralQ(memArea(S), q) << ',' << funcRel(opB);
				return 2;
			}
		case Brclr_S:
		case Brset_S:
		case Bsclr_S:
		case Bsset_S:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				_dst << bit(b, MemArea_COUNT, g_invalidAddress) << ',' << decode_dddddd(d) << ',' << funcRel(opB);
				return 2;
			}
		case Andi:
		case Ori:
			{
				const auto i = getFieldValue(inst, Field_iiiiiiii, op);
				const auto ee = getFieldValue(inst, Field_EE, op);
				_dst << immediate(i) << ',' << decodeEE(ee);
			}
			return 1;
		case Asl_ii:
		case Asr_ii:
			{
				const auto i = getFieldValue(inst, Field_iiiiii, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << immediate(i) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Asl_S1S2D:
		case Asr_S1S2D:
			{
				const auto sss = getFieldValue(inst, Field_sss, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Bcc_xxxx:
		case BScc_xxxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_line.opName += condition(cccc);
				_dst << funcRel(opB);
			}
			return 2;
		case Bcc_xxx: 
		case BScc_xxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				m_line.opName += condition(cccc);
				_dst << funcRel(a);
			}
			return 1;
		case Bcc_Rn: 
		case BScc_Rn:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto r = getFieldValue(inst, Field_RRR, op);
				m_line.opName += condition(cccc);
				_dst << decode_RRR(r);
			}
			return 1;
		case Bra_xxxx: 
		case Bsr_xxxx:
			_dst << funcRel(opB);
			return 2;
		case Bra_xxx: 
		case Bsr_xxx:
			{
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				_dst << funcRel(a);
			}
			return 1;
		case Bra_Rn: 
		case Bsr_Rn:
			{
				const auto r = getFieldValue(inst, Field_RRR, op);
				_dst << decode_RRR(r);
			}
			return 1;
		case BRKcc:
		case Debugcc:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_line.opName += condition(cccc);
			}
			return 1;
		case Clb:
			{
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Cmpu_S1S2:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto ggg = getFieldValue(inst, Field_ggg, op);
				_dst << decode_alu_GGG(ggg, d) << ',' << aluD(d);
			}
			return 1;
		case Div:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto jj = getFieldValue(inst, Field_JJ, op);
				_dst << decode_JJ_read(jj) << ',' << aluD(d);
			}
			return 1;
		case Dmac:
			{
				const auto ss			= getFieldValue<Dmac,Field_S, Field_s>(op);
				const bool ab			= getFieldValue<Dmac,Field_d>(op);
				const bool negate		= getFieldValue<Dmac,Field_k>(op);

				const TWord qqqq		= getFieldValue<Dmac,Field_QQQQ>(op);

				_dst << (ss > 1 ? 'u' : 's');
				_dst << (ss > 0 ? 'u' : 's');
				_dst << ' ';
				if(negate)
					_dst << '-';
				_dst << decode_QQQQ(qqqq) << ',' << aluD(ab);
			}
			return 1;
		case Do_ea: 
			{
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << mmmrrr(mr, S, opB) << ',' << absAddr(MemArea_P, opB + 1, true);
			}
			return 2;
		case Dor_ea:
			{
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << mmmrrr(mr, S, opB) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Do_aa:
			{
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << absAddr(memArea(S), a, false, true) << ',' << absAddr(MemArea_P, opB + 1, true, false);
			}
			return 2;
		case Dor_aa:
			{
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << absAddr(memArea(S), a, false, true) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Do_xxx: 
			{
				const auto a	= getFieldValue(inst, Field_hhhh, Field_iiiiiiii, op);
				_dst << immediateShort(a) << ',' << absAddr(MemArea_P, opB + 1, true, false);
			}
			return 2;
		case Dor_xxx:
			{
				const auto a	= getFieldValue(inst, Field_hhhh, Field_iiiiiiii, op);
				_dst << immediateShort(a) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Do_S: 
			{
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				_dst << decode_dddddd(d) << ',' << absAddr(MemArea_P, opB + 1, true, false);
			}
			return 2;
		case Dor_S:
			{
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				_dst << decode_dddddd(d) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case DoForever:
			{
				_dst << absAddr(MemArea_P, opB + 1, true, false);
			}
			return 2;
		case DorForever:
			{
				_dst << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Extract_S1S2:
		case Extractu_S1S2:
			{
				const auto sss = getFieldValue(inst, Field_SSS, op);
				const auto s = getFieldValue(inst, Field_s, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Extract_CoS2: 
		case Extractu_CoS2:
			{
				const auto s = getFieldValue(inst, Field_s, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << immediate(opB) << ',' << aluD(s) << ',' << aluD(d);				
			}
			return 2;
		case Insert_S1S2:
			{
				const auto sss = getFieldValue(inst, Field_SSS, op);
				const auto qqq = getFieldValue(inst, Field_qqq, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << decode_sss(sss) << ',' << decode_qqq(qqq) << ',' << aluD(d);
			}
			return 1;
		case Insert_CoS2:
			{
				const auto qq = getFieldValue(inst, Field_qqq, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << immediate(opB) << ',' << decode_qqq(qq) << ',' << aluD(d);
			}
			return 2;
		case Ifcc:
		case Ifcc_U:
			{				
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				_dst << "if" << condition(cccc);
				if(inst == Ifcc_U)
					_dst << ".u";
			}
			return 1;
		case Jcc_xxx: 
		case Jscc_xxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = getFieldValue(inst, Field_aaaaaaaaaaaa, op);
				m_line.opName += condition(cccc);
				_dst << funcAbs(a);
			}
			return 1;
		case Jcc_ea: 
		case Jscc_ea:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto mr = getFieldValue(inst, Field_MMM, Field_RRR, op);
				m_line.opName += condition(cccc);
				_dst << mmmrrr(mr, MemArea_P, opB, false, false, true);
			}
			return 1 + m_extWordUsed;
		case Jclr_ea:
		case Jset_ea:
		case Jsclr_ea:
		case Jsset_ea:
			{
				const auto b		= getFieldValue(inst, Field_bbbbb, op);
				const auto mr		= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S		= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), mmmrrrAddr(mr, opB)) << ',' << mmmrrr(mr, S, opB) << ',' << funcAbs(opB);
			}
			return 2;
		case Jclr_aa:
		case Jset_aa:
		case Jsclr_aa:
		case Jsset_aa:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), a) << ',' << absAddr(memArea(S), a, false, false) << ',' << funcAbs(opB);
			}
			return 2;
		case Jclr_pp:
		case Jsclr_pp:
		case Jset_pp:
		case Jsset_pp:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto p	= getFieldValue(inst, Field_pppppp, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), peripheralPaddr(p)) << ',' << peripheralP(memArea(S), p) << ',' << funcAbs(opB);
			}
			return 2;
		case Jclr_qq:
		case Jsclr_qq:
		case Jset_qq:
		case Jsset_qq:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto q	= getFieldValue(inst, Field_qqqqqq, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				_dst << bit(b, memArea(S), peripheralQaddr(q)) << ',' << peripheralQ(memArea(S), q) << ',' << funcAbs(opB);
			}
			return 2;
		case Jclr_S:
		case Jsclr_S:
		case Jset_S:
		case Jsset_S:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				_dst << bit(b, MemArea_COUNT, g_invalidAddress) << ',' << decode_dddddd(d) << ',' << funcAbs(opB);
			}
			return 2;
		case Jmp_ea: 
		case Jsr_ea:
			{
				const auto mr = getFieldValue(inst, Field_MMM, Field_RRR, op);
				_dst << mmmrrr(mr, MemArea_P, opB, false, false, true);
			}
			return 1 + m_extWordUsed;
		case Jmp_xxx:
		case Jsr_xxx:
			{
				const auto a = getFieldValue(inst, Field_aaaaaaaaaaaa, op);
				_dst << funcAbs(a);
			}
			return 1;

		case Lra_Rn:
			{
				const auto rr = getFieldValue(inst, Field_RRR, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				_dst << decode_RRR(rr) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Lra_xxxx: 
			{
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				_dst << relativeAddr(MemArea_P, opB) << ',' << decode_DDDDDD(dd);
			}
			return 2;
		case Lsl_ii: 
		case Lsr_ii:
			{
				const auto i = getFieldValue(inst, Field_iiiii, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << immediate(i) << ',' << aluD(d);
			}
			return 1;
		case Lsl_SD:
		case Lsr_SD:
			{
				const auto ss = getFieldValue(inst, Field_sss, op);
				const auto d = getFieldValue(inst, Field_D, op);
				_dst << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Lua_ea:
			{
				const auto mr = getFieldValue(inst, Field_MM, Field_RRR, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);				
				_dst << mmmrrr(mr, MemArea_P, opB, false) << ',' << decode_DDDDDD(dd);
			}
			return 1 + m_extWordUsed;
		case Lua_Rn: 
			{
				const auto aa = signextend<int,7>(getFieldValue(inst, Field_aaa, Field_aaaa, op));
				const auto rr = getFieldValue(inst, Field_RRR, op);				
				const auto dd = getFieldValue(inst, Field_dddd, op);

				const auto index = dd & 7;
				const auto reg = dd < 8 ? 'r' : 'n';

				_dst << decode_RRR(rr, aa) << ',' << reg << index;
			}
			return 1;
		case Mac_S:
		case Macr_S:
		case Mpy_SD:
		case Mpyr_SD:
			{
				const int sssss		= getFieldValue(inst,Field_sssss,op);
				const TWord QQ		= getFieldValue(inst,Field_QQ,op);
				const bool ab		= getFieldValue(inst,Field_d,op);
				const bool negate	= getFieldValue(inst,Field_k,op);

				if(negate)
					_dst << '-';
				_dst << decode_QQ_read(QQ) << ',' << immediate(sssss) << ',' << aluD(ab);
			}
			return 1;
		case Maci_xxxx:
		case Macri_xxxx:
		case Mpyi:
		case Mpyri:
			{
				const TWord QQ		= getFieldValue(inst,Field_qq,op);
				const bool ab		= getFieldValue(inst,Field_d,op);
				const bool negate	= getFieldValue(inst,Field_k,op);

				if(negate)
					_dst << '-';
				_dst << immediateLong(opB) << ',' << decode_qq_read(QQ) << ',' << aluD(ab);
			}
			return 2;
		case Macsu:
		case Mpy_su:
			{
				const bool ab		= getFieldValue(inst,Field_d,op);
				const bool negate	= getFieldValue(inst,Field_k,op);
				const bool uu		= getFieldValue(inst,Field_s,op);
				const TWord qqqq	= getFieldValue(inst,Field_QQQQ,op);

				m_line.opName += (uu ? "uu" : "su");
				if(negate)
					_dst << '-';

				_dst << decode_QQQQ(qqqq) << ',' << aluD(ab);
			}
			return 1;
		case Merge:
			{
				const auto d		= getFieldValue(inst,Field_D,op);
				const auto ss		= getFieldValue(inst,Field_SSS,op);
				_dst << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Move_xx:
			{
				const auto ii = getFieldValue(inst, Field_iiiiiiii, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				_dst << immediate(ii) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Mover: 
			{
				const auto ee = getFieldValue(inst, Field_eeeee, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				_dst << decode_DDDDDD(ee) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Move_ea: 
			{
				const auto mr = getFieldValue<Move_ea, Field_MM, Field_RRR>(op);
				_dst << mmmrrr(mr, MemArea_P, op, false);
			}
			return 1 + m_extWordUsed;
		case Movex_ea:
		case Movey_ea:
			{
				const TWord mr	= getFieldValue<Movex_ea,Field_MMM, Field_RRR>(op);
				const TWord dd	= getFieldValue<Movex_ea,Field_dd, Field_ddd>(op);
				const TWord w	= getFieldValue<Movex_ea,Field_W>(op);

				auto d = decode_dddddd(dd);
				auto ea = mmmrrr(mr, inst == Movey_ea ? 1 : 0, opB);
				if(w)
					_dst << ea << ',' << d;
				else
					_dst << d << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movex_aa:
		case Movey_aa:
			{
				const TWord aa	= signextend<int,8>(getFieldValue<Movex_aa,Field_aaaaaa>(op));
				const TWord dd	= getFieldValue<Movex_aa,Field_dd, Field_ddd>(op);
				const TWord w	= getFieldValue<Movex_aa,Field_W>(op);
				const auto area = inst == Movey_aa ? MemArea_Y : MemArea_X;
				
				auto d = decode_dddddd(dd);
				if(w)
					_dst << absAddr(area, aa, false, true) << ',' << d;
				else
					_dst << d << ',' << absAddr(area, aa, false, true);
			}
			return 1;
		case Movex_Rnxxxx: 
		case Movey_Rnxxxx:
			{
				const TWord rr	= getFieldValue<Movex_Rnxxxx,Field_RRR>(op);
				const TWord dd	= getFieldValue<Movex_Rnxxxx,Field_DDDDDD>(op);
				const TWord w	= getFieldValue<Movex_Rnxxxx,Field_W>(op);

				const auto r = decode_RRR(rr, opB);
				const auto d = decode_dddddd(dd);

				const char* area = memArea(inst == Movey_Rnxxxx ? MemArea_Y : MemArea_X);

				if(w)
					_dst << area << r << ',' << d;
				else
					_dst << d << ',' << area << r;				
			}
			return 2;
		case Movex_Rnxxx:
		case Movey_Rnxxx:
			{
				const TWord rr	= getFieldValue<Movex_Rnxxx,Field_RRR>(op);
				const TWord dd	= getFieldValue<Movex_Rnxxx,Field_DDDD>(op);
				const TWord w	= getFieldValue<Movex_Rnxxx,Field_W>(op);
				const TWord aa	=  signextend<int,7>(getFieldValue<Movex_Rnxxx,Field_aaaaaa, Field_a>(op));

				const auto r = decode_RRR(rr, aa);
				const auto d = decode_DDDDDD(dd);

				const char* area = memArea(inst == Movey_Rnxxx ? MemArea_Y : MemArea_X);

				if(w)
					_dst << area << r << ',' << decode_DDDDDD(dd);
				else
					_dst << decode_DDDDDD(dd) << ',' << area << r;				
			}
			return 1;
		case Movexr_ea:
			{
				const TWord F		= getFieldValue<Movexr_ea,Field_F>(op);	// true:Y1, false:Y0
				const TWord mr		= getFieldValue<Movexr_ea,Field_MMM, Field_RRR>(op);
				const TWord ff		= getFieldValue<Movexr_ea,Field_ff>(op);
				const TWord write	= getFieldValue<Movexr_ea,Field_W>(op);
				const TWord d		= getFieldValue<Movexr_ea,Field_d>(op);

				const auto ea = mmmrrr(mr, MemArea_X, opB, true);

				const auto* const ee = decode_ee(ff);
				
				// S1/D1 move
				if( write )
				{
					m_line.m_moveA << ea << ',' << ee;
				}
				else
				{
					m_line.m_moveA << ee << ',' << ea;
				}

				// S2 D2 move
				const auto* const ab = aluD(d);
				const auto* const f  = F ? "y1" : "y0";

				m_line.m_moveB << ab << ',' << f;
			}
			return 1 + m_extWordUsed;
		case Movexr_A: 
			{
				const TWord mr		= getFieldValue<Movexr_A,Field_MMM, Field_RRR>(op);
				const TWord d		= getFieldValue<Movexr_A,Field_d>(op);

				const auto ea = mmmrrr(mr, MemArea_X, opB);
				const auto* const ab = aluD(d);

				// S1/D1 move
				m_line.m_moveA << ab << ',' << ea;

				// S2 D2 move
				m_line.m_moveB << "x" << "0," << ab;
			}
			return 1 + m_extWordUsed;
		case Moveyr_A:
			{
				const TWord mr		= getFieldValue<Movexr_A,Field_MMM, Field_RRR>(op);
				const TWord d		= getFieldValue<Movexr_A,Field_d>(op);

				const auto ea = mmmrrr(mr, MemArea_Y, opB);
				const auto* const ab = aluD(d);

				// S2 D2 move
				m_line.m_moveA << "y" << "0," << ab;

				// S1/D1 move
				m_line.m_moveB << ab << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Moveyr_ea:
			{
				const TWord e		= getFieldValue<Moveyr_ea,Field_e>(op);	// true:Y1, false:Y0
				const TWord mr		= getFieldValue<Moveyr_ea,Field_MMM, Field_RRR>(op);
				const TWord ff		= getFieldValue<Moveyr_ea,Field_ff>(op);
				const TWord write	= getFieldValue<Moveyr_ea,Field_W>(op);
				const TWord d		= getFieldValue<Moveyr_ea,Field_d>(op);

				const auto ea = mmmrrr(mr, MemArea_Y, opB, true);

				const auto* const ee = decode_ff(ff);

				// S2 D2 move
				const auto* const ab = aluD(d);
				const auto* const f  = e ? "x1" : "x0";

				m_line.m_moveA << ab << ',' << f;
				
				// S1/D1 move
				if( write )
				{
					m_line.m_moveB << ea << ',' << ee;
				}
				else
				{
					m_line.m_moveB << ee << ',' << ea;
				}
			}
			return 1 + m_extWordUsed;
		case Movel_ea:
			{
				const TWord ll = getFieldValue<Movel_ea,Field_L, Field_LL>(op);
				const TWord mr = getFieldValue<Movel_ea,Field_MMM, Field_RRR>(op);
				const auto w   = getFieldValue<Movel_ea,Field_W>(op);

				const auto ea = mmmrrr(mr, g_memArea_L, opB, false);
				const auto* l = decode_LLL(ll);

				if(w)
					_dst << "l:" << ea << ',' << l;
				else
					_dst << l << ',' << "l:" << ea;
			}
			return 1 + m_extWordUsed;
		case Movel_aa:
			{
				const TWord ll = getFieldValue<Movel_aa,Field_L, Field_LL>(op);
				const TWord aa = getFieldValue<Movel_aa,Field_aaaaaa>(op);
				const auto w   = getFieldValue<Movel_aa,Field_W>(op);
				
				const auto* l = decode_LLL(ll);
				const auto ea = absShortAddr(g_memArea_L, aa);
				if(w)
					_dst << "l:" << ea << ',' << l;
				else
					_dst << l << ',' << "l:" << ea;
			}
			return 1;
		case Movexy:
			{
				const TWord mrX		= getFieldValue<Movexy,Field_MM, Field_RRR>(op);
				TWord mmY			= getFieldValue<Movexy,Field_mm>(op);
				TWord rrY			= getFieldValue<Movexy,Field_rr>(op);
				const TWord writeX	= getFieldValue<Movexy,Field_W>(op);
				const TWord	writeY	= getFieldValue<Movexy,Field_w>(op);
				const TWord	ee		= getFieldValue<Movexy,Field_ee>(op);
				const TWord ff		= getFieldValue<Movexy,Field_ff>(op);

				rrY += ((mrX&0x7) >= 4) ? 0 : 4;

				const auto mrY = mmY << 3 | rrY;

				const auto eaX = mmrrr(mrX);
				const auto eaY = mmrrr(mrY);

				// X
				if( writeX )	m_line.m_moveA << "x:" << eaX << ',' << decode_ee(ee);
				else			m_line.m_moveA << decode_ee(ee) << ',' << "x:" << eaX;

				// Y
				if( writeY )	m_line.m_moveB << "y:" << eaY << ',' << decode_ff(ff);
				else			m_line.m_moveB << decode_ff(ff) << ',' << "y:" << eaY;
			}
			return 1;
		case Movec_ea:
			{
				const TWord dd	= getFieldValue<Movec_ea,Field_DDDDD>(op);
				const TWord mr	= getFieldValue<Movec_ea,Field_MMM, Field_RRR>(op);
				const auto w	= getFieldValue<Movec_ea,Field_W>(op);
				const auto s	= getFieldValue<Movec_ea,Field_S>(op);

				const auto ea = mmmrrr(mr, s, opB, true);

				if(w)		_dst << ea << ',' << decode_ddddd_pcr(dd);
				else		_dst << decode_ddddd_pcr(dd) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movec_aa: 
			{
				const TWord dd	= getFieldValue<Movec_aa,Field_DDDDD>(op);
				const TWord aa	= getFieldValue<Movec_aa,Field_aaaaaa>(op);
				const auto w	= getFieldValue<Movec_aa,Field_W>(op);
				const auto s	= getFieldValue<Movec_aa,Field_S>(op);

				const auto ea = absAddr(memArea(s), aa, false, true);

				if(w)		_dst << ea << ',' << decode_ddddd_pcr(dd);
				else		_dst << decode_ddddd_pcr(dd) << ',' << ea;
			}
			return 1;
		case Movec_S1D2: 
			{
				const TWord dd	= getFieldValue<Movec_S1D2,Field_DDDDD>(op);
				const TWord ee	= getFieldValue<Movec_S1D2,Field_eeeeee>(op);
				const auto w	= getFieldValue<Movec_S1D2,Field_W>(op);
				
				const auto ea = decode_dddddd(ee);

				if(w)		_dst << ea << ',' << decode_ddddd_pcr(dd);
				else		_dst << decode_ddddd_pcr(dd) << ',' << ea;
			}
			return 1;
		case Movec_xx: 
			{
				const TWord dd	= getFieldValue<Movec_xx,Field_DDDDD>(op);
				const TWord ii	= getFieldValue<Movec_xx,Field_iiiiiiii>(op);

				const auto ea = decode_DDDDDD(dd);

				_dst << immediate(ii) << ',' << decode_ddddd_pcr(dd);
			}
			return 1;
		case Movem_ea: 
			{
				const TWord dd	= getFieldValue<Movem_ea,Field_dddddd>(op);
				const TWord mr	= getFieldValue<Movem_ea,Field_MMM, Field_RRR>(op);
				const auto w	= getFieldValue<Movem_ea,Field_W>(op);

				const auto reg = decode_dddddd(dd);
				const auto ea = mmmrrr(mr, MemArea_P, opB);

				if(w)
					_dst << ea << ',' << reg;
				else
					_dst << reg << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movem_aa:
			{
				const TWord dd	= getFieldValue<Movem_aa,Field_dddddd>(op);
				const TWord aa	= getFieldValue<Movem_aa,Field_aaaaaa>(op);
				const auto w	= getFieldValue<Movem_aa,Field_W>(op);

				const auto reg = decode_dddddd(dd);
				const auto ea = memArea(MemArea_P) + absShortAddr(MemArea_P, aa);

				if(w)
					_dst << ea << ',' << reg;
				else
					_dst << reg << ',' << ea;
			}
			return 1;
		case Movep_ppea:
			{
				const TWord pp		= getFieldValue<Movep_ppea,Field_pppppp>(op);
				const TWord mr		= getFieldValue<Movep_ppea,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_ppea,Field_W>(op);
				const EMemArea s	= getFieldValue<Movep_ppea,Field_s>(op) ? MemArea_Y : MemArea_X;
				const EMemArea S	= getFieldValueMemArea<Movep_ppea>(op);

				const auto ea		= mmmrrr(mr, S, opB, true);

				if( write )
				{
					createBitsComment(mmmrrrAddr(mr, opB), s, peripheralPaddr(pp));
					_dst << ea << ',' << peripheralP(s, pp);
				}
				else
				{
					_dst << peripheralP(s, pp) << ',' << ea;
				}
			}
			return 1 + m_extWordUsed;
		case Movep_Xqqea: 
		case Movep_Yqqea:
			{
				const TWord qq		= getFieldValue<Movep_Xqqea,Field_qqqqqq>(op);
				const TWord mr		= getFieldValue<Movep_Xqqea,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_Xqqea,Field_W>(op);
				const EMemArea s	= inst == Movep_Yqqea ? MemArea_Y : MemArea_X;
				const EMemArea S	= getFieldValueMemArea<Movep_Xqqea>(op);

				const auto ea		= mmmrrr(mr, S, opB, true);

				if( write )
				{
					createBitsComment(mmmrrrAddr(mr, opB), s, peripheralQaddr(qq));
					_dst << ea << ',' << peripheralQ(s, qq);
				}
				else
					_dst << peripheralQ(s, qq) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movep_eapp: 
			{
				const TWord pp		= getFieldValue<Movep_eapp,Field_pppppp>(op);
				const TWord mr		= getFieldValue<Movep_eapp,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_eapp,Field_W>(op);
				const EMemArea s	= getFieldValue<Movep_eapp,Field_s>(op) ? MemArea_Y : MemArea_X;

				const auto ea		= mmmrrr(mr, MemArea_P, opB, true);

				if( write )
				{
					createBitsComment(mmmrrrAddr(mr, opB), s, peripheralPaddr(pp));
					_dst << ea << ',' << peripheralP(s, pp);
				}
				else
					_dst << peripheralP(s, pp) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movep_eaqq: 
			{
				const TWord qq		= getFieldValue<Movep_eaqq,Field_qqqqqq>(op);
				const TWord mr		= getFieldValue<Movep_eaqq,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_eaqq,Field_W>(op);
				const EMemArea S	= getFieldValueMemArea<Movep_eaqq>(op);

				const auto ea		= mmmrrr(mr, MemArea_P, opB, true);

				if( write )
					_dst << ea << ',' << peripheralQ(S, qq);
				else
					_dst << peripheralQ(S, qq) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movep_Spp: 
			{
				const TWord pp		= getFieldValue<Movep_Spp,Field_pppppp>(op);
				const TWord dd		= getFieldValue<Movep_Spp,Field_dddddd>(op);
				const auto write	= getFieldValue<Movep_Spp,Field_W>(op);
				const EMemArea S	=  getFieldValue<Movep_Spp,Field_s>(op) ? MemArea_Y : MemArea_X;

				const auto ea		= decode_dddddd(dd);

				if( write )
					_dst << ea << ',' << peripheralP(S, pp);
				else
					_dst << peripheralP(S, pp) << ',' << ea;
			}
			return 1;
		case Movep_SXqq:
		case Movep_SYqq:
			{
				const TWord qq		= getFieldValue<Movep_SXqq,Field_q, Field_qqqqq>(op);
				const TWord dd		= getFieldValue<Movep_SXqq,Field_dddddd>(op);
				const auto write	= getFieldValue<Movep_SXqq,Field_W>(op);
				const EMemArea S	= inst == Movep_SYqq ? MemArea_Y : MemArea_X;

				const auto ea		= decode_dddddd(dd);

				if( write )
					_dst << ea << ',' << peripheralQ(S, qq);
				else
					_dst << peripheralQ(S, qq) << ',' << ea;
			}
			return 1;
		case Norm:
			{
				const auto rr = getFieldValue<Norm,Field_RRR>(op);
				const auto d = getFieldValue<Norm,Field_d>(op);
				_dst << decode_RRR(rr) << ',' << aluD(d);
			}
			return 1;
		case Normf: 
			{
				const auto ss = getFieldValue<Normf,Field_sss>(op);
				const auto d = getFieldValue<Normf,Field_D>(op);
				_dst << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Plock:
		case Punlock:
			{
				const TWord mr = getFieldValue<Plock, Field_MMM, Field_RRR>(op);
				_dst << mmmrrr(mr, MemArea_P, opB, false, false);
			}
			return 1 + m_extWordUsed;
		case Plockr:
		case Punlockr:
			{
				_dst << relativeAddr(MemArea_P, opB);
			}
			return 2;
		case Rep_ea:
			{
				const TWord mr = getFieldValue<Rep_ea, Field_MMM, Field_RRR>(op);
				const auto S = getFieldValueMemArea<Rep_ea>(op);
				_dst << mmmrrr(mr, S, opB);
			}
			return 1 + m_extWordUsed;
		case Rep_aa: 
			{
				const TWord aa = getFieldValue<Rep_aa, Field_aaaaaa>(op);
				const auto S = getFieldValueMemArea<Rep_aa>(op);
				_dst << absAddr(S, aa, false, false);
			}
			return 1;
		case Rep_xxx: 
			{
				const TWord ii = getFieldValue<Rep_xxx, Field_hhhh, Field_iiiiiiii>(op);
				_dst << immediateShort(ii);
			}
			return 1;
		case Rep_S: 
			{
				const auto dd = getFieldValue<Rep_S, Field_dddddd>(op);
				_dst << decode_dddddd(dd);
			}
			return 1;
		case Tcc_S1D1:
			{
				const auto cc = getFieldValue<Tcc_S1D1, Field_CCCC>(op);
				const auto jj = getFieldValue<Tcc_S1D1, Field_JJJ>(op);
				const auto d  = getFieldValue<Tcc_S1D1, Field_d>(op);

				const auto* j = aluJJJ(jj,d);

				m_line.opName += condition(cc);
				_dst << j << ',' << aluD(d);
			}
			break;
		case Tcc_S1D1S2D2:
			{
				const auto cc = getFieldValue<Tcc_S1D1S2D2, Field_CCCC>(op);
				const auto jj = getFieldValue<Tcc_S1D1S2D2, Field_JJJ>(op);
				const auto tt = getFieldValue<Tcc_S1D1S2D2, Field_ttt>(op);
				const auto TT = getFieldValue<Tcc_S1D1S2D2, Field_TTT>(op);
				const auto d  = getFieldValue<Tcc_S1D1S2D2, Field_d>(op);

				const auto* j = aluJJJ(jj,d);

				m_line.opName += condition(cc);
				_dst << j << ',' << aluD(d) << ' ' << decode_TTT(tt) << ',' << decode_TTT(TT);
			}
			break;
		case Tcc_S2D2:
			{
				const auto cc = getFieldValue<Tcc_S2D2, Field_CCCC>(op);
				const auto tt = getFieldValue<Tcc_S2D2, Field_ttt>(op);
				const auto TT = getFieldValue<Tcc_S2D2, Field_TTT>(op);

				m_line.opName += condition(cc);
				_dst << decode_TTT(tt) << ',' << decode_TTT(TT);
			}
			break;
		case Trapcc:
			{
				const auto cc = getFieldValue<Trapcc, Field_CCCC>(op);
				m_line.opName += condition(cc);
			}
			break;
		case Vsl:
			{
				const TWord mr = getFieldValue<Vsl, Field_MMM, Field_RRR>(op);
				const auto s = getFieldValue<Vsl, Field_S>(op);
				const auto i = getFieldValue<Vsl, Field_i>(op);

				const auto ea = mmmrrr(mr, g_memArea_L, opB, false);
				_dst << aluD(s) << ',' << i << ",l:" << ea;
			}
			return 1 + m_extWordUsed;

		// Opcodes without parameters
		case Debug: 
		case Enddo:
		case Illegal:
		case Nop:
		case Pflush:
		case Pflushun:
		case Pfree:
		case Reset:
		case Rti:
		case Rts:
		case Stop:
		case Trap:
		case Wait:
			return 1;
		case Move_Nop:
		case Parallel:
		case InstructionCount:
		case ResolveCache:
		default: 
			return 0;
		}
		return 1;
	}

	uint32_t Disassembler::disassembleParallelMove(std::stringstream& _dst, const OpcodeInfo& oi, TWord _op, TWord _opB)
	{
		return disassemble(_dst, oi, _op, _opB);
	}

	uint32_t Disassembler::disassembleNonParallel(std::stringstream& _dst, const OpcodeInfo& oi, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
		return disassemble(_dst, oi, op, opB);
	}

	void Disassembler::createBitsComment(TWord _bits, EMemArea _area, TWord _addr)
	{
		if(!_bits || _bits == g_invalidAddress || _bits == g_dynamicAddress)
			return;

		const auto& s = m_bitSymbols[getSymbolType(_area)];
		const auto it = s.find(_addr);

		std::stringstream ss;
		ss << "(bits:";

		for(int b = 23; b >= 0; --b)
		{
			if((_bits & (1<<b)) == 0)
				continue;

			ss << ' ';

			bool found = false;
			if(it != s.end())
			{
				const auto& bits = it->second.bits;
				const auto itBit = bits.find(1<<b);
				if(itBit != bits.end())
				{
					ss << itBit->second;
					found = true;
				}
			}
			if(!found)
				ss << '$' << std::hex << b;
		}

		if(it != s.end())
		{
			for (const auto & itBit : it->second.bits)
			{
				auto bit = itBit.first;

				if((bit & (bit-1)) == 0)
					continue;

				auto res = bit & _bits;

				while((bit & 1) == 0)
				{
					res >>= 1;
					bit >>= 1;
				}

				ss << ' ' << itBit.second << "=$" << std::hex << res;
			}
		}
		m_line.m_comment << ss.str() << ")";
	}

	uint32_t Disassembler::disassemble(std::string& dst, TWord op, TWord opB, const TWord sr, const TWord omr, const TWord pc)
	{
		Line line;
		const auto res = disassemble(line, op, opB, sr, omr, pc);
		dst = formatLine(line);
		return res;
	}

	uint32_t Disassembler::disassemble(Line& dst, TWord op, TWord opB, const TWord sr, const TWord omr, const TWord pc)
	{
		m_extWordUsed = 0;
		m_pc = pc;

		m_line = Line();
		m_line.pc = pc;
		m_line.opA = op;
		m_line.opB = opB;

		auto finalize = [&](uint32_t res)
		{
			m_line.len = res;
			std::swap(dst, m_line);
			return res;
		};

		auto finalizeDC = [&]()
		{
			disassembleDC(m_line, op);
			return finalize(0);
		};

		if(!op)
		{
			m_line.opName = g_opNames[Nop];
			m_line.instA = Nop;
			return finalize(1);
		}

		if(Opcodes::isNonParallelOpcode(op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);
			if(oi)
			{
				m_line.opName = g_opNames[oi->m_instruction];
				m_line.instA = oi->m_instruction;
				const auto res = disassembleNonParallel(m_line.m_alu, *oi, op, opB, sr, omr);
				if(!res)
					return finalizeDC();
				return finalize(res);
			}

			return finalizeDC();
		}

		const auto* oiMove = m_opcodes.findParallelMoveOpcodeInfo(op);

		if(!oiMove)
			return finalizeDC();

		const OpcodeInfo* oiAlu = nullptr;

		if(op & 0xff)
		{
			oiAlu = m_opcodes.findParallelAluOpcodeInfo(op);
			if(!oiAlu)
				return finalizeDC();
		}
		else if(!oiAlu)
		{
			if(oiMove->getInstruction() == Ifcc || oiMove->getInstruction() == Ifcc_U)
				return finalizeDC();

			m_line.opName = g_opNames[oiMove->m_instruction];
			m_line.instA = oiMove->m_instruction;
			const auto resMove = disassembleParallelMove(m_line.m_moveA, *oiMove, op, opB);
			if(resMove)
				return finalize(resMove);
			return finalizeDC();
		}

		switch (oiMove->m_instruction)
		{
		case Move_Nop:
			{
				m_line.opName = g_opNames[oiAlu->m_instruction];
				m_line.instA = oiAlu->m_instruction;
				const auto resAlu = disassembleAlu(m_line.m_alu, oiAlu->m_instruction, op & 0xff);
				if(resAlu)
					return finalize(resAlu);
				return finalizeDC();
			}
		default:
			{
				m_line.opName = g_opNames[oiAlu->m_instruction];
				m_line.instA = oiAlu->m_instruction;
				m_line.instB = oiMove->m_instruction;

				const auto resAlu = disassembleAlu(m_line.m_alu, oiAlu->m_instruction, op & 0xff);
				const auto resMove = disassembleParallelMove(m_line.m_moveA, *oiMove, op, opB);
				return finalize(std::max(resMove, resAlu));
			}
		}
	}

	bool Disassembler::getSymbol(const SymbolType _type, const TWord _key, std::string& _result) const
	{
		const auto& symbols = m_symbols[_type];
		const auto it = symbols.find(_key);
		if(it != symbols.end())
		{
			_result = it->second;
			return true;
		}
		return false;
	}

	bool Disassembler::getSymbol(EMemArea _area, TWord _key, std::string& _result) const
	{
		const auto type = getSymbolType(_area);
		if(type == SymbolTypeCount)
			return false;

		return getSymbol(type, _key, _result);
	}

	Disassembler::SymbolType Disassembler::getSymbolType(EMemArea _area)
	{
		switch (_area)
		{
		case MemArea_X: return MemX;
		case MemArea_Y: return MemY;
		case MemArea_P: return MemP;
		case g_memArea_L: return MemL;
		}
		return SymbolTypeCount;
	}

	bool Disassembler::addSymbol(const SymbolType _type, const TWord _key, const std::string& _value)
	{
		auto& symbols = m_symbols[_type];
		const auto it = symbols.find(_key);
		if(it != symbols.end())
			return false;

		symbols.insert(std::make_pair(_key, _value));

		return true;
	}

	bool Disassembler::addBitSymbol(SymbolType _type, TWord _address, TWord _bit, const std::string& _symbol)
	{
		return addBitMaskSymbol(_type, _address, 1<<_bit, _symbol);
	}

	bool Disassembler::addBitMaskSymbol(SymbolType _type, TWord _address, TWord _bit, const std::string& _symbol)
	{
		auto& symbols = m_bitSymbols[_type];

		const auto it = symbols.find(_address);

		if(it != symbols.end())
		{
			BitSymbols& bs = it->second;
			bs.bits[_bit] = _symbol;
			return true;
		}

		BitSymbols bs;
		bs.bits.insert(std::make_pair(_bit, _symbol));
		symbols.insert(std::make_pair(_address, bs));

		return true;
	}

	bool Disassembler::addSymbols(const Memory& _mem)
	{
		const auto& symbols = _mem.getSymbols();

		auto success = false;
		
		for (const auto& symbol : symbols)
		{
			SymbolType type = SymbolTypeCount;

			switch (symbol.first)
			{
			case 'x':
			case 'X':
				type = MemX;
				break;
			case 'y':
			case 'Y':
				type = MemY;
				break;
			case 'p':
			case 'P':
				type = MemP;
				break;
			case 'l':
			case 'L':
				type = MemL;
				break;
			case 'n':
			case 'N':
				type = Immediate;
				break;
			default:
				break;
			}

			if(type != SymbolTypeCount)
			{
				for (const auto& it : symbol.second)
				{
					const auto& names = it.second.names;
					if(names.size() == 1)
						success |= addSymbol(type, it.first, *names.begin());
				}
			}
		}

		return success;
	}

	uint32_t disassembleMotorola(char* _dst, TWord _op, TWord _opB, const TWord _sr, const TWord _omr)
	{
#ifdef USE_MOTOROLA_UNASM
		unsigned long ops[3] = {_op, _opB, 0};
		const int ret = dspt_unasm_563( ops, _dst, _sr, _omr, nullptr );
		return ret;
#endif
		_dst[0] = 0;
		return 0;
	}

	void testDisassembler()
	{
#ifdef USE_MOTOROLA_UNASM
		LOG("Executing Disassembler Unit Tests");

		Opcodes opcodes;
		Disassembler disasm(opcodes);

		constexpr TWord opB = 0x234567;

		for (TWord i = 0x000000; i <= 0xffffff; ++i)
		{
			const TWord op = i;

			if ((i & 0x03ffff) == 0)
				LOG("Testing opcodes $" << HEX(i) << "+");

			char assemblyMotorola[128];
			std::string assembly;

			const auto opcodeCountMotorola = disassembleMotorola(assemblyMotorola, op, opB, 0, 0);

			if (opcodeCountMotorola > 0)
			{
				const int opcodeCount = disasm.disassemble(assembly, op, opB, 0, 0, 0);

				if (!opcodeCount)
					continue;

				std::string asmMot(assemblyMotorola);

				if (asmMot.back() == ' ')	// debugcc has extra space
					asmMot.pop_back();

				if (assembly != asmMot)
				{
					switch (op)
					{
					case 0x000006: if (assembly == "trap" && asmMot == "swi")                       continue; break;
					case 0x202015: if (assembly == "maxm a,b ifcc"   && asmMot == "max b,a ifcc")   continue; break;
					case 0x202115: if (assembly == "maxm a,b ifge"   && asmMot == "max b,a ifge")   continue; break;
					case 0x202215: if (assembly == "maxm a,b ifne"   && asmMot == "max b,a ifne")   continue; break;
					case 0x202315: if (assembly == "maxm a,b ifpl"   && asmMot == "max b,a ifpl")   continue; break;
					case 0x202415: if (assembly == "maxm a,b ifnn"   && asmMot == "max b,a ifnn")   continue; break;
					case 0x202515: if (assembly == "maxm a,b ifec"   && asmMot == "max b,a ifec")   continue; break;
					case 0x202615: if (assembly == "maxm a,b iflc"   && asmMot == "max b,a iflc")   continue; break;
					case 0x202715: if (assembly == "maxm a,b ifgt"   && asmMot == "max b,a ifgt")   continue; break;
					case 0x202815: if (assembly == "maxm a,b ifcs"   && asmMot == "max b,a ifcs")   continue; break;
					case 0x202915: if (assembly == "maxm a,b iflt"   && asmMot == "max b,a iflt")   continue; break;
					case 0x202a15: if (assembly == "maxm a,b ifeq"   && asmMot == "max b,a ifeq")   continue; break;
					case 0x202b15: if (assembly == "maxm a,b ifmi"   && asmMot == "max b,a ifmi")   continue; break;
					case 0x202c15: if (assembly == "maxm a,b ifnr"   && asmMot == "max b,a ifnr")   continue; break;
					case 0x202d15: if (assembly == "maxm a,b ifes"   && asmMot == "max b,a ifes")   continue; break;
					case 0x202e15: if (assembly == "maxm a,b ifls"   && asmMot == "max b,a ifls")   continue; break;
					case 0x202f15: if (assembly == "maxm a,b ifle"   && asmMot == "max b,a ifle")   continue; break;
					case 0x203015: if (assembly == "maxm a,b ifcc.u" && asmMot == "max b,a ifcc.u") continue; break;
					case 0x203115: if (assembly == "maxm a,b ifge.u" && asmMot == "max b,a ifge.u") continue; break;
					case 0x203215: if (assembly == "maxm a,b ifne.u" && asmMot == "max b,a ifne.u") continue; break;
					case 0x203315: if (assembly == "maxm a,b ifpl.u" && asmMot == "max b,a ifpl.u") continue; break;
					case 0x203415: if (assembly == "maxm a,b ifnn.u" && asmMot == "max b,a ifnn.u") continue; break;
					case 0x203515: if (assembly == "maxm a,b ifec.u" && asmMot == "max b,a ifec.u") continue; break;
					case 0x203615: if (assembly == "maxm a,b iflc.u" && asmMot == "max b,a iflc.u") continue; break;
					case 0x203715: if (assembly == "maxm a,b ifgt.u" && asmMot == "max b,a ifgt.u") continue; break;
					case 0x203815: if (assembly == "maxm a,b ifcs.u" && asmMot == "max b,a ifcs.u") continue; break;
					case 0x203915: if (assembly == "maxm a,b iflt.u" && asmMot == "max b,a iflt.u") continue; break;
					case 0x203a15: if (assembly == "maxm a,b ifeq.u" && asmMot == "max b,a ifeq.u") continue; break;
					case 0x203b15: if (assembly == "maxm a,b ifmi.u" && asmMot == "max b,a ifmi.u") continue; break;
					case 0x203c15: if (assembly == "maxm a,b ifnr.u" && asmMot == "max b,a ifnr.u") continue; break;
					case 0x203d15: if (assembly == "maxm a,b ifes.u" && asmMot == "max b,a ifes.u") continue; break;
					case 0x203e15: if (assembly == "maxm a,b ifls.u" && asmMot == "max b,a ifls.u") continue; break;
					case 0x203f15: if (assembly == "maxm a,b ifle.u" && asmMot == "max b,a ifle.u") continue; break;
					}

					LOG("Diff for opcode " << HEX(op) << ": " << assembly << " != " << assemblyMotorola);
					assert(false);
					disasm.disassemble(assembly, op, opB, 0, 0, 0);	// retry to help debugging
				}

				if (opcodeCount != opcodeCountMotorola)
				{
					LOG("Diff for opcode " << HEX(op) << ": " << assembly << ": instruction length " << opcodeCount << " != " << opcodeCountMotorola);
					disasm.disassemble(assembly, op, opB, 0, 0, 0);	// retry to help debugging
				}
			}
		}

		LOG("Disassembler Unit Tests completed");
#endif
	}
}
