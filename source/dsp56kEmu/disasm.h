#pragma once

#if defined(DSP56K_USE_MOTOROLA_UNASM) && (!defined(_WIN32) || !defined(_DEBUG) || defined(_WIN64))
#	undef USE_MOTOROLA_UNASM
#endif

#include <array>
#include <map>
#include <sstream>
#include <string>

#include "opcodes.h"

namespace dsp56k
{
	class Memory;
	struct OpcodeInfo;
	
	class Disassembler
	{
	public:
		enum SymbolType
		{
			MemX,
			MemY,
			MemP,
			Immediate,

			SymbolTypeCount
		};

		struct Symbol
		{
			SymbolType type;
			std::string name;
		};

		Disassembler(const Opcodes& _opcodes);

		int disassemble(std::string& dst, TWord op, TWord opB, TWord sr, TWord omr, TWord pc);

		bool addSymbol(SymbolType _type, TWord _key, const std::string& _value);
		bool addSymbols(const Memory& _mem);
		const std::map<TWord,std::string>& getSymbols(const SymbolType _type) const { return m_symbols[_type]; }

	private:
		int disassembleAlu(std::string& _dst, const OpcodeInfo& _oiAlu, TWord op);
		int disassembleAlu(Instruction inst, TWord op);
		int disassembleParallelMove(const OpcodeInfo& oi, TWord _op, TWord _opB);
		int disassemble(const OpcodeInfo& oi, TWord op, TWord opB);
		void disassembleDC(std::string& dst, TWord op);

		int disassembleNonParallel(const OpcodeInfo& oi, TWord op, TWord opB, TWord sr, TWord omr);

		std::string mmmrrr(TWord mmmrrr, TWord S, TWord opB, bool _addMemSpace = true, bool _long = true);

		bool getSymbol(SymbolType _type, TWord _key, std::string& _result) const;
		bool getSymbol(EMemArea _area, TWord _key, std::string& _result) const;

		std::string immediate(const char* _prefix, TWord _data) const;
		std::string immediate(TWord _data) const;
		std::string immediateShort(TWord _data) const;
		std::string immediateLong(TWord _data) const;
		std::string relativeAddr(EMemArea _area, int _data, bool _long = false) const;
		std::string relativeLongAddr(EMemArea _area, int _data) const;
		std::string absAddr(EMemArea _area, int _data, bool _long = false) const;

		std::string peripheral(TWord S, TWord a, TWord _root) const;
		std::string peripheralQ(TWord S, TWord a) const;
		std::string peripheralP(TWord S, TWord a) const;

		std::string absShortAddr(EMemArea _area, int _data) const;

		const Opcodes& m_opcodes;

		std::stringstream m_ss;
		std::string m_str;

		uint32_t m_extWordUsed = 0;
		TWord m_pc = 0;

		std::array<std::map<TWord,std::string>, SymbolTypeCount> m_symbols;
	};

	uint32_t disassembleMotorola( char* _dst, TWord _opA, TWord _opB, TWord _sr, TWord _omr);
}
