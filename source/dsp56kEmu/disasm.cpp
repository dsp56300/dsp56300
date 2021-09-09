#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

#include "disasm.h"

#include <algorithm>

#include "logging.h"
#include "memory.h"
#include "peripherals.h"
#include "utils.h"

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
		"abs ",
		"adc ",
		"add ",		"add ",		"add ",
		"addl ",	"addr ",
		"and ",		"and ",		"and ",		"andi ",
		"asl ",		"asl ",		"asl ",
		"asr ",		"asr ",		"asr ",
		"b",		"b",		"b",
		"bchg ",	"bchg ",	"bchg ",	"bchg ",	"bchg ",
		"bclr ",	"bclr ",	"bclr ",	"bclr ",	"bclr ",
		"bra ",		"bra ",		"bra ",
		"brclr ",	"brclr ",	"brclr ",	"brclr ",	"brclr ",
		"brk",
		"brset ",	"brset ",	"brset ",	"brset ",	"brset ",
		"bs",		"bs",		"bs",
		"bsclr ",	"bsclr ",	"bsclr ",	"bsclr ",	"bsclr ",
		"bset ",	"bset ",	"bset ",	"bset ",	"bset ",
		"bsr ",		"bsr ",		"bsr ",
		"bsset ",	"bsset ",	"bsset ",	"bsset ",	"bsset ",
		"btst ",	"btst ",	"btst ",	"btst ",	"btst ",
		"clb ",
		"clr ",
		"cmp ",		"cmp ",		"cmp ",
		"cmpm ",	"cmpu ",
		"debug",	"debug",
		"dec ",
		"div ",
		"dmac",
		"do ",		"do ",		"do ",		"do ",
		"do forever,",
		"dor ",		"dor ",		"dor ",		"dor ",
		"dor forever,",
		"enddo",
		"eor ",		"eor ",		"eor ",
		"extract ",	"extract ",
		"extractu ","extractu ",
		"",			"",																						// ifcc
		"illegal",	"inc ",
		"insert ",	"insert ",
		"j",		"j",
		"jclr ",	"jclr ",	"jclr ",	"jclr ",	"jclr ",
		"jmp ",		"jmp ",
		"js",		"js",
		"jsclr ",	"jsclr ",	"jsclr ",	"jsclr ",	"jsclr ",
		"jset ",	"jset ",	"jset ",	"jset ",	"jset ",
		"jsr ",		"jsr ",
		"jsset ",	"jsset ",	"jsset ",	"jsset ",	"jsset ",
		"lra ",		"lra ",
		"lsl ",		"lsl ",		"lsl ",
		"lsr ",		"lsr ",		"lsr ",
		"lua ",		"lua ",
		"mac ",		"mac ",
		"maci ",
		"mac",
		"macr ",	"macr ",
		"macri ",
		"max ",
		"maxm ",
		"merge ",
		"move ",	"move ",																				// movenop, move_xx
		"move ",																							// move_xx
		"move ",																							// move_ea
		"move ",	"move ",	"move ",	"move ",														// movex
		"move ",	"move ",																				// movexr
		"move ",	"move ",	"move ",	"move ",														// movey
		"move ",	"move ",																				// moveyr
		"move ",	"move ",																				// movel
		"move ",																							// movexy
		"move ",	"move ",	"move ",	"move ",														// movec
		"move ",	"move ",																				// movem
		"movep ",	"movep ",	"movep ",	"movep ",	"movep ",	"movep ",	"movep ",	"movep ",		// movep
		"mpy ",		"mpy ",		"mpy",
		"mpyi ",
		"mpyr ",	"mpyr ",
		"mpyri ",
		"neg ",
		"nop",
		"norm ",
		"normf ",
		"not ",
		"or ",		"or ",		"or ",
		"ori ",
		"pflush",
		"pflushun",
		"pfree",
		"plock ",
		"plockr ",
		"punlock ",
		"punlockr ",
		"rep ",		"rep ",		"rep ",		"rep ",
		"reset",
		"rnd ",
		"rol ",
		"ror ",
		"rti",
		"rts",
		"sbc ",
		"stop",
		"sub ",		"sub ",		"sub ",
		"subl ",
		"subr ",
		"t",		"t",		"t",
		"tfr ",
		"trap",
		"trap",
		"tst ",
		"vsl ",
		"wait",
		"resolvecache",
		"parallel",
	};

	static_assert(sizeof(g_opNames) / sizeof(g_opNames[0]) == InstructionCount, "operation names entries missing or too many");

	const char* g_conditionCodes[16] =	{"cc", "ge", "ne", "pl", "nn", "ec", "lc", "gt", "cs", "lt", "eq", "mi", "nr", "es", "ls", "le"};
	
	std::string hex(TWord _data)
	{
		std::stringstream ss; ss << '$' << std::hex << _data; return std::string(ss.str());
	}

	std::string Disassembler::immediate(const char* _prefix, TWord _data) const
	{
		std::string sym;
		if(getSymbol(Immediate, _data, sym))
			return sym;

		return std::string(_prefix) + hex(_data);
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

	std::string Disassembler::absAddr(EMemArea _area, int _data, bool _long) const
	{
		std::string sym;
		if(getSymbol(_area, _data, sym))
			return sym;

		if(!_long)
			return hex(_data);
		return ">" + hex(_data);
	}

	std::string Disassembler::absShortAddr(EMemArea _area, int _data) const
	{
		std::string sym;
		if(getSymbol(_area, _data, sym))
			return sym;

		return "<" + hex(_data);
	}

	const char* aluD(bool ab)
	{
		return ab ? "b" : "a";
	}

	void disassembleDC(std::stringstream& dst, TWord op)
	{
		dst << "dc $" << HEX(op);
	}

	Disassembler::Disassembler(const Opcodes& _opcodes) : m_opcodes(_opcodes)
	{
		m_str.reserve(1024);
	}

	void Disassembler::disassembleDC(std::string& dst, TWord op)
	{
		dsp56k::disassembleDC(m_ss, op);
		dst = m_ss.str();
	}

	bool Disassembler::disassembleMemoryBlock(std::string& dst, const std::vector<uint32_t>& _memory, TWord _pc, bool _skipNops, bool _writePC, bool _writeOpcodes)
	{
		if (_memory.empty())
			return false;

		std::stringstream out;

		for (TWord i = 0; i < _memory.size();)
		{
			const TWord a = _memory[i];
			const TWord b = i + 1 < _memory.size() ? _memory[i + 1] : 0;

			if (_skipNops && !a)
			{
				++i;
				continue;
			}

			std::string assem;
			const auto len = disassemble(assem, a, b, 0, 0, i);

			if(_writePC)
				out << '$' << HEX(i + _pc) << " - ";

			if(_writeOpcodes)
			{
				if (len == 2)
					out << '$' << HEX(a) << ' ' << '$' << HEX(b);
				else
					out << '$' << HEX(a) << "        ";
			}

			out << " = " << assem << std::endl;

			if (!len)
				++i;
			else
				i += len;
		}

		dst = std::string(out.str());;
		return true;
	}

	int Disassembler::disassembleAlu(std::string& _dst, const OpcodeInfo& _oiAlu, TWord op)
	{
		const int res = disassembleAlu(_oiAlu.m_instruction, op);
		if(res)
		{
			_dst = m_ss.str();
			return res;
		}
		disassembleDC(_dst, op);
		return 0;
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
		}
		return "?";
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
		}
		return nullptr;
	}

	EMemArea memArea(TWord _area)
	{
		if(_area > 1)
			return MemArea_P;
		return _area ? MemArea_Y : MemArea_X;
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

	std::string memory(TWord S, TWord a)
	{
		const auto area = memArea(S);
		return std::string(memArea(area)) + '<' + hex(a);
	}

	std::string Disassembler::peripheral(EMemArea area, TWord a, TWord _root) const
	{
		std::string sym;
		if(getSymbol(area, _root + a, sym))
			return sym;

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

	std::string Disassembler::mmmrrr(TWord _mmmrrr, TWord S, TWord opB, bool _addMemSpace, bool _long)
	{
		return mmmrrr(_mmmrrr, memArea(S), opB, _addMemSpace, _long);
	}

	std::string Disassembler::mmmrrr(TWord mmmrrr, EMemArea S, TWord opB, bool _addMemSpace, bool _long)
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
		case 0x30:	// absolute address
			++m_extWordUsed;
			if(opB >= XIO_Reserved_High_First)
				return peripheral(S, opB, 0xffff80);
			return area + absAddr(S, opB, _long);
		case 0x34:	// immediate data
			++m_extWordUsed;
			return immediateLong(opB);
		default:
			{
				const auto mmm = (mmmrrr>>3)&7;
				const auto r = mmmrrr & 7;
				const auto* format = formats[mmm];
				if(!format)
					return std::string();
				sprintf(temp, format, r, r);
			}
			return area + temp;
		}
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
		}
		return "?";
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
		}
		return "?";
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
		}
		return "?";
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
		}
		return "?";
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
			return std::string();

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
		}
		return "?";
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
		}
		return "?";
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
		}
		return "?,?";
	}
	
	int Disassembler::disassembleAlu(Instruction inst, dsp56k::TWord op)
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
				m_ss << aluD(D);
				break;
			case Max:
			case Maxm:
				m_ss << "a,b";
				break;
			default:
				const char* j = aluJJJ(JJJ, D);

				if(!j)
				{
					m_ss << aluD(D);
				}
				else
				{
					m_ss << j << "," << aluD(D);
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
			m_ss << '-';

		m_ss << q << "," << aluD(D);

		return 1;
	}

	const char* decode_JJ_read( TWord jj )
	{
		switch( jj )
		{
		case 0: return "x0";
		case 1: return "y0";
		case 2: return "x1";
		case 3: return "y1";
		}
		return "?";
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
		}
		return "?,?";
	}

	const char* decode_QQ_read( TWord _qq )
	{
		switch( _qq )
		{
		case 0:		return "y1";
		case 1:		return "x0";
		case 2:		return "y0";
		case 3:		return "x1";
		}
		return "?";
	}

	const char* decode_qq_read( TWord _qq )
	{
		switch( _qq )
		{
		case 0:		return "x0";
		case 1:		return "y0";
		case 2:		return "x1";
		case 3:		return "y1";
		}
		return "?";
	}

	const char* decode_ee( TWord _ee )
	{
		switch( _ee )
		{
		case 0:	return "x0";
		case 1:	return "x1";
		case 2:	return "a";
		case 3:	return "b";
		}
		return "?";
	}

	const char* decode_ff( TWord _ff)
	{
		switch( _ff )
		{
		case 0:	return "y0";
		case 1:	return "y1";
		case 2:	return "a";
		case 3:	return "b";
		}
		return "?";
	}

	std::string decode_TTT(TWord _ttt)
	{
		char temp[3] = {'r', 0, 0};
		temp[1] = '0' + _ttt;
		return temp;
	}

	int Disassembler::disassemble(const OpcodeInfo& oi, TWord op, TWord opB)
	{
		const auto inst = oi.m_instruction;

		// switch-case of death incoming in 3..2..1...

		switch (inst)
		{
		// ALU operations without any other variables
		case Dec:
		case Inc:
			{
				const auto d = getFieldValue(inst, Field_d, op);				
				m_ss << aluD(d);
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
				m_ss << immediateShort(i) << ',' << aluD(d);
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
				m_ss << immediateLong(opB) << ',' << aluD(d);
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
				m_ss << immediate(b) << ',' << mmmrrr(mr, S, opB);
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
				m_ss << immediate(b) << ',' << memory(S, a);
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
				m_ss << immediate(b) << ',' << peripheralP(memArea(S), p);
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
				m_ss << immediate(b) << ',' << peripheralQ(memArea(S), q);
				return 1;
			}
		case Bchg_D:
		case Bclr_D:
		case Bset_D:
		case Btst_D:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << immediate(b) << ',' << decode_dddddd(d);
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
				m_ss << immediate(b) << ',' << mmmrrr(mr, S, opB) << ',' << relativeAddr(MemArea_P, opB);
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
				m_ss << immediate(b) << ',' << memory(S, a) << ',' << relativeAddr(MemArea_P, opB);
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
				m_ss << immediate(b) << ',' << peripheralP(memArea(S), p) << ',' << relativeAddr(MemArea_P, opB);
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
				m_ss << immediate(b) << ',' << peripheralQ(memArea(S), q) << ',' << relativeAddr(MemArea_P, opB);
				return 2;
			}
		case Brclr_S:
		case Brset_S:
		case Bsclr_S:
		case Bsset_S:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << immediate(b) << ',' << decode_dddddd(d) << ',' << relativeAddr(MemArea_P, opB);
				return 2;
			}
		case Andi:
		case Ori:
			{
				const auto i = getFieldValue(inst, Field_iiiiiiii, op);
				const auto ee = getFieldValue(inst, Field_EE, op);
				m_ss << immediate(i) << ',' << decodeEE(ee);
			}
			return 1;
		case Asl_ii:
		case Asr_ii:
			{
				const auto i = getFieldValue(inst, Field_iiiiii, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(i) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Asl_S1S2D:
		case Asr_S1S2D:
			{
				const auto sss = getFieldValue(inst, Field_sss, op);
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Bcc_xxxx:
		case BScc_xxxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_ss << condition(cccc) << ' ' << relativeAddr(MemArea_P, opB);
			}
			return 2;
		case Bcc_xxx: 
		case BScc_xxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				m_ss << condition(cccc) << ' ' << relativeLongAddr(MemArea_P, a);
			}
			return 1;
		case Bcc_Rn: 
		case BScc_Rn:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto r = getFieldValue(inst, Field_RRR, op);
				m_ss << condition(cccc) << ' ' << decode_RRR(r);
			}
			return 1;
		case Bra_xxxx: 
		case Bsr_xxxx:
			m_ss << relativeAddr(MemArea_P, opB);
			return 2;
		case Bra_xxx: 
		case Bsr_xxx:
			{
				const auto a = signextend<int,9>(getFieldValue(inst, Field_aaaa, Field_aaaaa, op));
				m_ss << relativeLongAddr(MemArea_P, a);
			}
			return 1;
		case Bra_Rn: 
		case Bsr_Rn:
			{
				const auto r = getFieldValue(inst, Field_RRR, op);
				m_ss << decode_RRR(r);
			}
			return 1;
		case BRKcc:
		case Debugcc:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				m_ss << condition(cccc);
			}
			return 1;
		case Clb:
			{
				const auto s = getFieldValue(inst, Field_S, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Cmpu_S1S2:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto ggg = getFieldValue(inst, Field_ggg, op);
				m_ss << decode_alu_GGG(ggg, d) << ',' << aluD(d);
			}
			return 1;
		case Div:
			{
				const auto d = getFieldValue(inst, Field_d, op);
				const auto jj = getFieldValue(inst, Field_JJ, op);
				m_ss << decode_JJ_read(jj) << ',' << aluD(d);
			}
			return 1;
		case Dmac:
			{
				const auto ss			= getFieldValue<Dmac,Field_S, Field_s>(op);
				const bool ab			= getFieldValue<Dmac,Field_d>(op);
				const bool negate		= getFieldValue<Dmac,Field_k>(op);

				const TWord qqqq		= getFieldValue<Dmac,Field_QQQQ>(op);

				m_ss << (ss > 1 ? 'u' : 's');
				m_ss << (ss > 0 ? 'u' : 's');
				m_ss << ' ';
				if(negate)
					m_ss << '-';
				m_ss << decode_QQQQ(qqqq) << ',' << aluD(ab);
			}
			return 1;
		case Do_ea: 
			{
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << mmmrrr(mr, S, opB) << ',' << hex(opB + 1);	// TODO: absolute address?!
			}
			return 2;
		case Dor_ea:
			{
				const auto mr	= getFieldValue(inst, Field_MMM, Field_RRR, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << mmmrrr(mr, S, opB) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Do_aa:
			{
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << memory(S, a) << ',' << hex(opB + 1);		// TODO: absolute adress?!
			}
			return 2;
		case Dor_aa:
			{
				const auto a	= getFieldValue(inst, Field_aaaaaa, op);
				const auto S	= getFieldValue(inst, Field_S, op);
				m_ss << memory(S, a) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Do_xxx: 
			{
				const auto a	= getFieldValue(inst, Field_hhhh, Field_iiiiiiii, op);
				m_ss << immediateShort(a) << ',' << hex(opB + 1);		// TODO: absolute address
			}
			return 2;
		case Dor_xxx:
			{
				const auto a	= getFieldValue(inst, Field_hhhh, Field_iiiiiiii, op);
				m_ss << immediateShort(a) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Do_S: 
			{
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << decode_dddddd(d) << ',' << hex(opB + 1);		// TODO: absolute address
			}
			return 2;
		case Dor_S:
			{
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << decode_dddddd(d) << ',' << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case DoForever:
			{
				m_ss << absAddr(MemArea_P, opB + 1);
			}
			return 2;
		case DorForever:
			{
				m_ss << relativeAddr(MemArea_P, opB + 1);
			}
			return 2;
		case Extract_S1S2:
		case Extractu_S1S2:
			{
				const auto sss = getFieldValue(inst, Field_SSS, op);
				const auto s = getFieldValue(inst, Field_s, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(sss) << ',' << aluD(s) << ',' << aluD(d);
			}
			return 1;
		case Extract_CoS2: 
		case Extractu_CoS2:
			{
				const auto s = getFieldValue(inst, Field_s, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(opB) << ',' << aluD(s) << ',' << aluD(d);				
			}
			return 2;
		case Insert_S1S2:
			{
				const auto sss = getFieldValue(inst, Field_SSS, op);
				const auto qqq = getFieldValue(inst, Field_qqq, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(sss) << ',' << decode_qqq(qqq) << ',' << aluD(d);
			}
			return 1;
		case Insert_CoS2:
			{
				const auto qq = getFieldValue(inst, Field_qqq, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(opB) << ',' << decode_qqq(qq) << ',' << aluD(d);
			}
			return 2;
		case Ifcc:
		case Ifcc_U:
			{				
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				if(m_ss.str().empty())
					m_ss << "move ";
				m_ss << "if" << condition(cccc);
				if(inst == Ifcc_U)
					m_ss << ".u";
			}
			return 1;
		case Jcc_xxx: 
		case Jscc_xxx:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto a = getFieldValue(inst, Field_aaaaaaaaaaaa, op);
				m_ss << condition(cccc) << ' ' << absShortAddr(MemArea_P, a);
			}
			return 1;
		case Jcc_ea: 
		case Jscc_ea:
			{
				const auto cccc = getFieldValue(inst, Field_CCCC, op);
				const auto mr = getFieldValue(inst, Field_MMM, Field_RRR, op);
				m_ss << condition(cccc) << ' ' << mmmrrr(mr, 2, opB, false);
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
				m_ss << immediate(b) << ',' << mmmrrr(mr, S, opB) << ',' << absAddr(MemArea_P, opB);
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
				m_ss << immediate(b) << ',' << memory(S, a) << ',' << absAddr(MemArea_P, opB);
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
				m_ss << immediate(b) << ',' << peripheralP(memArea(S), p) << ',' << absAddr(MemArea_P, opB);
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
				m_ss << immediate(b) << ',' << peripheralQ(memArea(S), q) << ',' << absAddr(MemArea_P, opB);
			}
			return 2;
		case Jclr_S:
		case Jsclr_S:
		case Jset_S:
		case Jsset_S:
			{
				const auto b	= getFieldValue(inst, Field_bbbbb, op);
				const auto d	= getFieldValue(inst, Field_DDDDDD, op);
				m_ss << immediate(b) << ',' << decode_dddddd(d) << ',' << absAddr(MemArea_P, opB);
			}
			return 2;
		case Jmp_ea: 
		case Jsr_ea:
			{
				const auto mr = getFieldValue(inst, Field_MMM, Field_RRR, op);
				m_ss << mmmrrr(mr, 2, opB, false);
			}
			return 1 + m_extWordUsed;
		case Jmp_xxx:
		case Jsr_xxx:
			{
				const auto a = getFieldValue(inst, Field_aaaaaaaaaaaa, op);
				m_ss << absShortAddr(MemArea_P, a);
			}
			return 1;

		case Lra_Rn:
			{
				const auto rr = getFieldValue(inst, Field_RRR, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				m_ss << decode_RRR(rr) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Lra_xxxx: 
			{
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				m_ss << relativeAddr(MemArea_P, opB) << ',' << decode_DDDDDD(dd);
			}
			return 2;
		case Lsl_ii: 
		case Lsr_ii:
			{
				const auto i = getFieldValue(inst, Field_iiiii, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << immediate(i) << ',' << aluD(d);
			}
			return 1;
		case Lsl_SD:
		case Lsr_SD:
			{
				const auto ss = getFieldValue(inst, Field_sss, op);
				const auto d = getFieldValue(inst, Field_D, op);
				m_ss << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Lua_ea:
			{
				const auto mr = getFieldValue(inst, Field_MM, Field_RRR, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);				
				m_ss << mmmrrr(mr, 2, opB, false) << ',' << decode_DDDDDD(dd);
			}
			return 1 + m_extWordUsed;
		case Lua_Rn: 
			{
				const auto aa = signextend<int,7>(getFieldValue(inst, Field_aaa, Field_aaaa, op));
				const auto rr = getFieldValue(inst, Field_RRR, op);				
				const auto dd = getFieldValue(inst, Field_dddd, op);

				const auto index = dd & 7;
				const auto reg = dd < 8 ? 'r' : 'n';

				m_ss << decode_RRR(rr, aa) << ',' << reg << index;
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
					m_ss << '-';
				m_ss << decode_QQ_read(QQ) << ',' << immediate(sssss) << ',' << aluD(ab);
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
					m_ss << '-';
				m_ss << immediateLong(opB) << ',' << decode_qq_read(QQ) << ',' << aluD(ab);
			}
			return 2;
		case Macsu:
		case Mpy_su:
			{
				const bool ab		= getFieldValue(inst,Field_d,op);
				const bool negate	= getFieldValue(inst,Field_k,op);
				const bool uu		= getFieldValue(inst,Field_s,op);
				const TWord qqqq	= getFieldValue(inst,Field_QQQQ,op);

				m_ss << (uu ? "uu" : "su") << ' ';
				if(negate)
					m_ss << '-';

				m_ss << decode_QQQQ(qqqq) << ',' << aluD(ab);
			}
			return 1;
		case Merge:
			{
				const auto d		= getFieldValue(inst,Field_D,op);
				const auto ss		= getFieldValue(inst,Field_SSS,op);
				m_ss << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Move_xx:
			{
				const auto ii = getFieldValue(inst, Field_iiiiiiii, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				m_ss << immediate(ii) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Mover: 
			{
				const auto ee = getFieldValue(inst, Field_eeeee, op);
				const auto dd = getFieldValue(inst, Field_ddddd, op);
				m_ss << decode_DDDDDD(ee) << ',' << decode_DDDDDD(dd);
			}
			return 1;
		case Move_ea: 
			{
				const auto mr = getFieldValue<Move_ea, Field_MM, Field_RRR>(op);
				m_ss << mmmrrr(mr, 2, op, false);
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
					m_ss << ea << ',' << d;
				else
					m_ss << d << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movex_aa:
		case Movey_aa:
			{
				const TWord aa	= signextend<int,8>(getFieldValue<Movex_aa,Field_aaaaaa>(op));
				const TWord dd	= getFieldValue<Movex_aa,Field_dd, Field_ddd>(op);
				const TWord w	= getFieldValue<Movex_aa,Field_W>(op);
				const auto s = inst == Movey_aa ? 1 : 0;
				
				auto d = decode_dddddd(dd);
				if(w)
					m_ss << memory(s,aa) << ',' << d;
				else
					m_ss << d << ',' << memory(s,aa);
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
					m_ss << area << r << ',' << d;
				else
					m_ss << d << ',' << area << r;				
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
					m_ss << area << r << ',' << decode_DDDDDD(dd);
				else
					m_ss << decode_DDDDDD(dd) << ',' << area << r;				
			}
			return 1;
		case Movexr_ea:
			{
				const TWord F		= getFieldValue<Movexr_ea,Field_F>(op);	// true:Y1, false:Y0
				const TWord mr		= getFieldValue<Movexr_ea,Field_MMM, Field_RRR>(op);
				const TWord ff		= getFieldValue<Movexr_ea,Field_ff>(op);
				const TWord write	= getFieldValue<Movexr_ea,Field_W>(op);
				const TWord d		= getFieldValue<Movexr_ea,Field_d>(op);

				const auto ea = mmmrrr(mr, 0, opB, true);

				const auto* const ee = decode_ee(ff);
				
				// S1/D1 move
				if( write )
				{
					m_ss << ea << ',' << ee;
				}
				else
				{
					m_ss << ee << ',' << ea;
				}

				// S2 D2 move
				const auto* const ab = aluD(d);
				const auto* const f  = F ? "y1" : "y0";

				m_ss << ' ' << ab << ',' << f;
			}
			return 1 + m_extWordUsed;
		case Movexr_A: 
			{
				const TWord mr		= getFieldValue<Movexr_A,Field_MMM, Field_RRR>(op);
				const TWord d		= getFieldValue<Movexr_A,Field_d>(op);

				const auto ea = mmmrrr(mr, 0, opB);
				const auto* const ab = aluD(d);

				// S1/D1 move
				m_ss <<  ab << ',' << ea;

				// S2 D2 move
				m_ss << ' ' << "x" << "0," << ab;
			}
			return 1 + m_extWordUsed;
		case Moveyr_A:
			{
				const TWord mr		= getFieldValue<Movexr_A,Field_MMM, Field_RRR>(op);
				const TWord d		= getFieldValue<Movexr_A,Field_d>(op);

				const auto ea = mmmrrr(mr, 1, opB);
				const auto* const ab = aluD(d);

				// S2 D2 move
				m_ss << "y" << "0," << ab;

				// S1/D1 move
				m_ss << ' ' << ab << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Moveyr_ea:
			{
				const TWord e		= getFieldValue<Moveyr_ea,Field_e>(op);	// true:Y1, false:Y0
				const TWord mr		= getFieldValue<Moveyr_ea,Field_MMM, Field_RRR>(op);
				const TWord ff		= getFieldValue<Moveyr_ea,Field_ff>(op);
				const TWord write	= getFieldValue<Moveyr_ea,Field_W>(op);
				const TWord d		= getFieldValue<Moveyr_ea,Field_d>(op);

				const auto ea = mmmrrr(mr, 1, opB, true);

				const auto* const ee = decode_ff(ff);

				// S2 D2 move
				const auto* const ab = aluD(d);
				const auto* const f  = e ? "x1" : "x0";

				m_ss << ab << ',' << f << ' ';
				
				// S1/D1 move
				if( write )
				{
					m_ss << ea << ',' << ee;
				}
				else
				{
					m_ss << ee << ',' << ea;
				}
			}
			return 1 + m_extWordUsed;
		case Movel_ea:
			{
				const TWord ll = getFieldValue<Movel_ea,Field_L, Field_LL>(op);
				const TWord mr = getFieldValue<Movel_ea,Field_MMM, Field_RRR>(op);
				const auto w   = getFieldValue<Movel_ea,Field_W>(op);

				const auto ea = mmmrrr(mr, 0, opB, false);
				const auto* l = decode_LLL(ll);

				if(w)
					m_ss << "l:" << ea << ',' << l;
				else
					m_ss << l << ',' << "l:" << ea;
			}
			return 1 + m_extWordUsed;
		case Movel_aa:
			{
				const TWord ll = getFieldValue<Movel_aa,Field_L, Field_LL>(op);
				const TWord aa = getFieldValue<Movel_aa,Field_aaaaaa>(op);
				const auto w   = getFieldValue<Movel_aa,Field_W>(op);
				
				const auto* l = decode_LLL(ll);
				const auto ea = absShortAddr(MemArea_X, aa);	// TODO: Memarea L
				if(w)
					m_ss << "l:" << ea << ',' << l;
				else
					m_ss << l << ',' << "l:" << ea;
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
				if( writeX )	m_ss << "x:" << eaX << ',' << decode_ee(ee);
				else			m_ss << decode_ee(ee) << ',' << "x:" << eaX;

				m_ss << ' ';

				// Y
				if( writeY )	m_ss << "y:" << eaY << ',' << decode_ff(ff);
				else			m_ss << decode_ff(ff) << ',' << "y:" << eaY;
			}
			return 1;
		case Movec_ea:
			{
				const TWord dd	= getFieldValue<Movec_ea,Field_DDDDD>(op);
				const TWord mr	= getFieldValue<Movec_ea,Field_MMM, Field_RRR>(op);
				const auto w	= getFieldValue<Movec_ea,Field_W>(op);
				const auto s	= getFieldValue<Movec_ea,Field_S>(op);

				const auto ea = mmmrrr(mr, s, opB, true);

				if(w)		m_ss << ea << ',' << decode_ddddd_pcr(dd);
				else		m_ss << decode_ddddd_pcr(dd) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movec_aa: 
			{
				const TWord dd	= getFieldValue<Movec_aa,Field_DDDDD>(op);
				const TWord aa	= getFieldValue<Movec_aa,Field_aaaaaa>(op);
				const auto w	= getFieldValue<Movec_aa,Field_W>(op);
				const auto s	= getFieldValue<Movec_aa,Field_S>(op);

				const auto ea = memory(s,aa);

				if(w)		m_ss << ea << ',' << decode_ddddd_pcr(dd);
				else		m_ss << decode_ddddd_pcr(dd) << ',' << ea;
			}
			return 1;
		case Movec_S1D2: 
			{
				const TWord dd	= getFieldValue<Movec_S1D2,Field_DDDDD>(op);
				const TWord ee	= getFieldValue<Movec_S1D2,Field_eeeeee>(op);
				const auto w	= getFieldValue<Movec_S1D2,Field_W>(op);
				
				const auto ea = decode_dddddd(ee);

				if(w)		m_ss << ea << ',' << decode_ddddd_pcr(dd);
				else		m_ss << decode_ddddd_pcr(dd) << ',' << ea;
			}
			return 1;
		case Movec_xx: 
			{
				const TWord dd	= getFieldValue<Movec_xx,Field_DDDDD>(op);
				const TWord ii	= getFieldValue<Movec_xx,Field_iiiiiiii>(op);

				const auto ea = decode_DDDDDD(dd);

				m_ss << immediate(ii) << ',' << decode_ddddd_pcr(dd);
			}
			return 1;
		case Movem_ea: 
			{
				const TWord dd	= getFieldValue<Movem_ea,Field_dddddd>(op);
				const TWord mr	= getFieldValue<Movem_ea,Field_MMM, Field_RRR>(op);
				const auto w	= getFieldValue<Movem_ea,Field_W>(op);

				const auto reg = decode_dddddd(dd);
				const auto ea = mmmrrr(mr, 2, opB);

				if(w)
					m_ss << ea << ',' << reg;
				else
					m_ss << reg << ',' << ea;
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
					m_ss << ea << ',' << reg;
				else
					m_ss << reg << ',' << ea;
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
					m_ss << ea << ',' << peripheralP(s, pp);
				else
					m_ss << peripheralP(s, pp) << ',' << ea;
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
					m_ss << ea << ',' << peripheralQ(s, qq);
				else
					m_ss << peripheralQ(s, qq) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movep_eapp: 
			{
				const TWord pp		= getFieldValue<Movep_eapp,Field_pppppp>(op);
				const TWord mr		= getFieldValue<Movep_eapp,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_eapp,Field_W>(op);
				const EMemArea s	= getFieldValue<Movep_eapp,Field_s>(op) ? MemArea_Y : MemArea_X;

				const auto ea		= mmmrrr(mr, 2, opB, true);

				if( write )
					m_ss << ea << ',' << peripheralP(s, pp);
				else
					m_ss << peripheralP(s, pp) << ',' << ea;
			}
			return 1 + m_extWordUsed;
		case Movep_eaqq: 
			{
				const TWord qq		= getFieldValue<Movep_eaqq,Field_qqqqqq>(op);
				const TWord mr		= getFieldValue<Movep_eaqq,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_eaqq,Field_W>(op);
				const EMemArea S	= getFieldValueMemArea<Movep_eaqq>(op);

				const auto ea		= mmmrrr(mr, 2, opB, true);

				if( write )
					m_ss << ea << ',' << peripheralQ(S, qq);
				else
					m_ss << peripheralQ(S, qq) << ',' << ea;
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
					m_ss << ea << ',' << peripheralP(S, pp);
				else
					m_ss << peripheralP(S, pp) << ',' << ea;
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
					m_ss << ea << ',' << peripheralQ(S, qq);
				else
					m_ss << peripheralQ(S, qq) << ',' << ea;
			}
			return 1;
		case Norm:
			{
				const auto rr = getFieldValue<Norm,Field_RRR>(op);
				const auto d = getFieldValue<Norm,Field_d>(op);
				m_ss << decode_RRR(rr) << ',' << aluD(d);
			}
			return 1;
		case Normf: 
			{
				const auto ss = getFieldValue<Normf,Field_sss>(op);
				const auto d = getFieldValue<Normf,Field_D>(op);
				m_ss << decode_sss(ss) << ',' << aluD(d);
			}
			return 1;
		case Plock:
		case Punlock:
			{
				const TWord mr = getFieldValue<Plock, Field_MMM, Field_RRR>(op);
				m_ss << mmmrrr(mr, 2, opB, false, false);
			}
			return 1 + m_extWordUsed;
		case Plockr:
		case Punlockr:
			{
				m_ss << relativeAddr(MemArea_P, opB);
			}
			return 2;
		case Rep_ea:
			{
				const TWord mr = getFieldValue<Rep_ea, Field_MMM, Field_RRR>(op);
				const auto S = getFieldValueMemArea<Rep_ea>(op);
				m_ss << mmmrrr(mr, S, opB);
			}
			return 1 + m_extWordUsed;
		case Rep_aa: 
			{
				const TWord aa = getFieldValue<Rep_aa, Field_aaaaaa>(op);
				const auto S = getFieldValueMemArea<Rep_aa>(op);
				m_ss << memory(S, aa);
			}
			return 1;
		case Rep_xxx: 
			{
				const TWord ii = getFieldValue<Rep_xxx, Field_hhhh, Field_iiiiiiii>(op);
				m_ss << immediateShort(ii);
			}
			return 1;
		case Rep_S: 
			{
				const auto dd = getFieldValue<Rep_S, Field_dddddd>(op);
				m_ss << decode_dddddd(dd);
			}
			return 1;
		case Tcc_S1D1:
			{
				const auto cc = getFieldValue<Tcc_S1D1, Field_CCCC>(op);
				const auto jj = getFieldValue<Tcc_S1D1, Field_JJJ>(op);
				const auto d  = getFieldValue<Tcc_S1D1, Field_d>(op);

				const auto* j = aluJJJ(jj,d);

				m_ss << condition(cc) << ' ' << j << ',' << aluD(d);
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

				m_ss << condition(cc) << ' ' << j << ',' << aluD(d) << ' ' << decode_TTT(tt) << ',' << decode_TTT(TT);
			}
			break;
		case Tcc_S2D2:
			{
				const auto cc = getFieldValue<Tcc_S2D2, Field_CCCC>(op);
				const auto tt = getFieldValue<Tcc_S2D2, Field_ttt>(op);
				const auto TT = getFieldValue<Tcc_S2D2, Field_TTT>(op);

				m_ss << condition(cc) << ' ' << decode_TTT(tt) << ',' << decode_TTT(TT);
			}
			break;
		case Trapcc:
			{
				const auto cc = getFieldValue<Trapcc, Field_CCCC>(op);
				m_ss << condition(cc);
			}
			break;
		case Vsl:
			{
				const TWord mr = getFieldValue<Vsl, Field_MMM, Field_RRR>(op);
				const auto s = getFieldValue<Vsl, Field_S>(op);
				const auto i = getFieldValue<Vsl, Field_i>(op);

				const auto ea = mmmrrr(mr, 0, opB, false);
				m_ss << aluD(s) << ',' << i << ",l:" << ea;
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

	int Disassembler::disassembleParallelMove(const OpcodeInfo& oi, TWord _op, TWord _opB)
	{
		return disassemble(oi, _op, _opB);
	}

	int Disassembler::disassembleNonParallel(const OpcodeInfo& oi, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
		return disassemble(oi, op, opB);
	}

	int Disassembler::disassemble(std::string& dst, TWord op, TWord opB, const TWord sr, const TWord omr, const TWord pc)
	{
		m_extWordUsed = 0;
		m_pc = pc;

		m_ss.str("");
		m_ss.clear();

		auto finalize = [&](int res)
		{
			dst = m_ss.str();
			return res;
		};

		if(!op)
		{
			dst = g_opNames[Nop];
			return 1;
		}

		if(Opcodes::isNonParallelOpcode(op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);
			if(oi)
			{
				m_ss << g_opNames[oi->m_instruction];
				const auto res = disassembleNonParallel(*oi, op, opB, sr, omr);
				if(!res)
				{
					disassembleDC(dst, op);
					return 0;
				}

				return finalize(res);
			}

			disassembleDC(dst, op);
			return 0;
		}

		const auto* oiMove = m_opcodes.findParallelMoveOpcodeInfo(op);

		if(!oiMove)
		{
			disassembleDC(dst, op);
			return 0;
		}

		const OpcodeInfo* oiAlu = nullptr;

		if(op & 0xff)
		{
			oiAlu = m_opcodes.findParallelAluOpcodeInfo(op);
			if(!oiAlu)
			{
				disassembleDC(dst, op);
				return 0;
			}
		}
		else if(!oiAlu)
		{
			m_ss << g_opNames[oiMove->m_instruction];
			const int resMove = disassembleParallelMove(*oiMove, op, opB);
			if(resMove)
				return finalize(resMove);
			disassembleDC(dst, op);
			return 0;
		}

		switch (oiMove->m_instruction)
		{
		case Move_Nop:
			{
				m_ss << g_opNames[oiAlu->m_instruction];
				const auto resAlu = disassembleAlu(oiAlu->m_instruction, op & 0xff);
				if(resAlu)
					return finalize(resAlu);
				disassembleDC(dst, op);
			}
			return 0;
		default:
			{
				m_ss << g_opNames[oiAlu->m_instruction];

				const auto resAlu = disassembleAlu(oiAlu->m_instruction, op & 0xff);
				m_ss << ' ';
				const auto resMove = disassembleParallelMove(*oiMove, op, opB);
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
		switch (_area)
		{
		case MemArea_X: return getSymbol(MemX, _key, _result);
		case MemArea_Y: return getSymbol(MemY, _key, _result);
		case MemArea_P: return getSymbol(MemP, _key, _result);
		default:
			return false;
		}
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

	bool Disassembler::addSymbols(const Memory& _mem)
	{
		const auto& symbols = _mem.getSymbols();

		auto success = false;
		
		for (const auto& symbol : symbols)
		{
			SymbolType type;

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
			default:
				type = Immediate;
				break;
			}

			for(auto it = symbol.second.begin(); it != symbol.second.end(); ++it)
			{
				success |= addSymbol(type, it->first, *it->second.names.begin());
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
}
