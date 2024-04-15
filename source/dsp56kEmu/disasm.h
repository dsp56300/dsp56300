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
			MemL,
			Immediate,

			SymbolTypeCount
		};

		struct Symbol
		{
			SymbolType type;
			std::string name;
		};

		struct BitSymbols
		{
			std::map<uint32_t, std::string> bits;
		};

		struct Line
		{
			std::string opName;
			std::stringstream m_alu;
			std::stringstream m_moveA;
			std::stringstream m_moveB;
			std::stringstream m_comment;
			Instruction instA = ResolveCache;
			Instruction instB = ResolveCache;
			TWord pc = 0;
			TWord len = 0;
			TWord opA = 0;
			TWord opB = 0;
		};

		explicit Disassembler(const Opcodes& _opcodes);

		static std::string formatLine(const Line& _line, bool _useColumnWidths = true);
		uint32_t disassemble(Line& dst, TWord op, TWord opB, TWord sr, TWord omr, TWord pc);
		uint32_t disassemble(std::string& dst, TWord op, TWord opB, TWord sr, TWord omr, TWord pc);

		bool addSymbol(SymbolType _type, TWord _key, const std::string& _value);
		bool addBitSymbol(SymbolType _type, TWord _address, TWord _bit, const std::string& _symbol);
		bool addBitMaskSymbol(SymbolType _type, TWord _address, TWord _bitMask, const std::string& _symbol);

		bool addSymbols(const Memory& _mem);
		const std::map<TWord,std::string>& getSymbols(const SymbolType _type) const { return m_symbols[_type]; }

		bool disassembleMemoryBlock(std::string& dst, const std::vector<uint32_t>& _memory, TWord _pc, bool _skipNops, bool _writePC, bool _writeOpcodes);
	
	private:
		static uint32_t disassembleAlu(std::stringstream& _dst, Instruction inst, TWord op);
		uint32_t disassembleParallelMove(std::stringstream& _dst, const OpcodeInfo& oi, TWord _op, TWord _opB);
		uint32_t disassemble(std::stringstream& _dst, const OpcodeInfo& oi, TWord op, TWord opB);
		static void disassembleDC(Line& dst, TWord op);

		uint32_t disassembleNonParallel(std::stringstream& _dst, const OpcodeInfo& oi, TWord op, TWord opB, TWord sr, TWord omr);

		void createBitsComment(TWord _bits, EMemArea _area, TWord _addr);
		std::string mmmrrr(TWord mmmrrr, TWord S, TWord opB, bool _addMemSpace = true, bool _long = true, bool _isJump = false);
		std::string mmmrrr(TWord mmmrrr, EMemArea S, TWord opB, bool _addMemSpace = true, bool _long = true, bool _isJump = false);
		static TWord mmmrrrAddr(TWord mmmrrr, TWord opB);

		bool getSymbol(SymbolType _type, TWord _key, std::string& _result) const;
		bool getSymbol(EMemArea _area, TWord _key, std::string& _result) const;
		static SymbolType getSymbolType(EMemArea _area);

		std::string bit(TWord _bit, EMemArea _area, TWord _referencingAddress) const;

		std::string immediate(const char* _prefix, TWord _data) const;
		std::string immediate(TWord _data) const;
		std::string immediateShort(TWord _data) const;
		std::string immediateLong(TWord _data) const;
		std::string relativeAddr(EMemArea _area, int _data, bool _long = false) const;
		std::string relativeLongAddr(EMemArea _area, int _data) const;
		std::string absAddr(EMemArea _area, int _data, bool _long = false, bool _withArea = true) const;
		bool functionName(std::string& _result, TWord _addr) const;
		std::string funcAbs(TWord _addr) const;
		std::string funcRel(TWord _addr) const;

		std::string peripheral(EMemArea S, TWord a, TWord _root) const;
		std::string peripheralQ(EMemArea S, TWord a) const;
		std::string peripheralP(EMemArea S, TWord a) const;
		static TWord peripheralPaddr(const TWord p);
		static TWord peripheralQaddr(const TWord q);

		std::string absShortAddr(EMemArea _area, int _data) const;

		const Opcodes& m_opcodes;

		Line m_line;

		uint32_t m_extWordUsed = 0;
		TWord m_pc = 0;

		std::array<std::map<TWord,std::string>, SymbolTypeCount> m_symbols;
		std::array<std::map<TWord,BitSymbols>, SymbolTypeCount> m_bitSymbols;
	};

	uint32_t disassembleMotorola( char* _dst, TWord _opA, TWord _opB, TWord _sr, TWord _omr);
	void testDisassembler();
}
