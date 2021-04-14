#if defined(_WIN32) && defined(_DEBUG)
#	define SUPPORT_DISASSEMBLER
#endif

#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

#include "disasm.h"

#include <algorithm>

#include "logging.h"

#ifdef SUPPORT_DISASSEMBLER
extern "C"
{
	#include "../dsp56k/PROTO563.H"
}
#endif

namespace dsp56k
{
	const char* g_opNames[InstructionCount] = 
	{
		"abs",
		"adc",
		"add",		"add",		"add",
		"addl",		"addr",
		"and",		"and",		"and",		"andi",
		"asl",		"asl",		"asl",
		"asr",		"asr",		"asr",
		"bcc",		"bcc",		"bcc",
		"bchg",		"bchg",		"bchg",		"bchg",		"bchg",
		"bclr",		"bclr",		"bclr",		"bclr",		"bclr",
		"bra",		"bra",		"bra",
		"brclr",	"brclr",	"brclr",	"brclr",	"brclr",
		"brkcc",
		"brset",	"brset",	"brset",	"brset",	"brset",
		"bscc",		"bscc",		"bscc",
		"bsclr",	"bsclr",	"bsclr",	"bsclr",	"bsclr",
		"bset",		"bset",		"bset",		"bset",		"bset",
		"bsr",		"bsr",		"bsr",
		"bsset",	"bsset",	"bsset",	"bsset",	"bsset",
		"btst",		"btst",		"btst",		"btst",		"btst",
		"clb",
		"clr",
		"cmp",		"cmp",		"cmp",
		"cmpm",		"cmpu",
		"debug",	"debugcc",
		"dec",
		"div",
		"dmac",
		"do",		"do",		"do",		"do",
		"doforever",
		"dor",		"dor",		"dor",		"dor",
		"dorforever",
		"enddo",
		"eor",		"eor",		"eor",
		"extract",	"extract",
		"extractu",	"extractu",
		"ifcc",		"ifcc",
		"illegal",	"inc",
		"insert",	"insert",
		"jcc",		"jcc",
		"jclr",		"jclr",		"jclr",		"jclr",		"jclr",
		"jmp",		"jmp",
		"jscc",		"jscc",
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
		"macsu",
		"macr",		"macr",
		"macri",
		"max",
		"maxm",
		"merge",
		"move",		"move",
		"mover",
		"move",
		"movex",	"movex",	"movex",		"movex",
		"movexr",	"movexr",
		"movey",	"movey",	"movey",		"movey",
		"moveyr",	"moveyr",
		"movel",	"movel",
		"movexy",
		"movec",	"movec",	"movec",		"movec",
		"movem",	"movem",
		"movep",	"movep",	"movep",		"movep",		"movep",		"movep",		"movep",		"movep",
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
		"tcc",		"tcc",		"tcc",
		"tfr",
		"trap",
		"trapcc",
		"tst",
		"vsl",
		"wait",
		"resolvecache",
		"parallel",
	};

	void disassembleDC(std::stringstream& dst, TWord op)
	{
		dst << "dc $" << HEX(op);
	}

	Disassembler::Disassembler()
	{
		m_str.reserve(1024);
	}

	void Disassembler::disassembleDC(std::string& dst, TWord op)
	{
		dsp56k::disassembleDC(m_ss, op);
		dst = m_ss.str();
	}

	int Disassembler::disassembleAlu(std::string& _dst, const OpcodeInfo& _oiAlu, TWord op)
	{
		const int res = disassembleAlu(_oiAlu, op);
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
			case 0:		return nullptr;
			case 1:		return ab ? "a" : "b";
			case 2:		return "x";
			case 3:		return "y";
			case 4:		return "x0";
			case 5:		return "y0";
			case 6:		return "x1";
			case 7:		return "y1";
		}
		return nullptr;
	}

	const char* aluQQQ(TWord QQQ)
	{
		switch (QQQ)
		{
		case 0:	return "x0,x0";
		case 1:	return "y0,y0";
		case 2:	return "x1,x0";
		case 3:	return "y1,y0";
		case 4:	return "x0,x1";
		case 5:	return "y0,x0";
		case 6:	return "x1,x0";
		case 7:	return "y1,xx";
		}
		return nullptr;
	}

	int Disassembler::disassembleAlu(const dsp56k::OpcodeInfo& oi, dsp56k::TWord op)
	{
		const auto D = (op>>3) & 0x1;
		const char* ab = D ? "b" : "a";

		if(op>>7 == 0)
		{
			// non-multiply
			const auto kkk = op & 0x7;
			const auto JJJ = (op>>4) & 0x7;

			if(!kkk && !JJJ)
				return 0;

			const char* j = aluJJJ(JJJ, D);

			if(!j)
			{
				m_ss << ab;
			}
			else
			{
				m_ss << j << "," << ab;
			}

			return 1;
		}

		// multiply
		const auto QQQ = (op>>4) & 0x7;
		const char* q = aluQQQ(QQQ);
		if(!q)
			return 0;

		m_ss << q << "," << ab;

		return 1;
	}

	int Disassembler::disassembleParallelMove(const OpcodeInfo& oi, TWord _op, TWord _opB)
	{
		return 1;
	}

	int Disassembler::disassembleNonParallel(const OpcodeInfo& oi, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
		return 1;
	}

	int Disassembler::disassemble(std::string& dst, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
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
				m_ss << g_opNames[oi->m_instruction] << ' ';
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
			m_ss << g_opNames[oiMove->m_instruction] << ' ';
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
				m_ss << g_opNames[oiAlu->m_instruction] << ' ';
				const auto resAlu = disassembleAlu(*oiAlu, op & 0xff);
				if(resAlu)
					return finalize(resAlu);
				disassembleDC(dst, op);
			}
			return 0;
		default:
			{
				m_ss << g_opNames[oiAlu->m_instruction] << ' ';

				const auto resAlu = disassembleAlu(*oiAlu, op & 0xff);
				const auto resMove = disassembleParallelMove(*oiMove, op, opB);
				return finalize(std::max(resMove, resAlu));
			}
		}
	}
	
	uint32_t disassemble(char* dst, TWord op, TWord opB, const TWord sr, const TWord omr)
	{
#ifdef SUPPORT_DISASSEMBLER
		unsigned long ops[3] = {op, opB, 0};
		const int ret = dspt_unasm_563( ops, dst, sr, omr, nullptr );
//		return ret;
#endif
		static Disassembler disassembler;

		std::string assembly;
		const int res = disassembler.disassemble(assembly, op, opB, sr, omr);
		strcpy(dst, assembly.c_str());
		return res;
	}
}
