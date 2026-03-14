#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "opcodes.h"
#include "opcodetypes.h"
#include "types.h"

namespace dsp56k
{
	enum class AssembleError
	{
		OK = 0,
		InvalidInstruction,
		InvalidRegister,
		InvalidAddressingMode,
		InvalidAddress,
		ValueOutOfRange,
		InvalidBitNumber,
		InvalidConditionCode,
		InvalidMemoryArea,
		InvalidImmediate,
		InvalidOperandCount,
		InvalidSyntax,
		AmbiguousInstruction,
		InternalError
	};

	struct AssembleResult
	{
		TWord word[2] = {0, 0};
		uint32_t wordCount = 0;
		AssembleError error = AssembleError::OK;

		bool success() const { return error == AssembleError::OK && wordCount > 0; }
	};

	class Assembler
	{
	public:
		Assembler();

		AssembleResult assemble(const char* _text) const;

	private:
		using Tokens = std::vector<std::string>;

		// Parsing helpers
		static std::string toLower(const std::string& _s);
		static Tokens tokenize(const std::string& _text);
		static bool splitOperands(const std::string& _operands, std::vector<std::string>& _result);

		// Register encoding
		static bool parseRegister_dddddd(const std::string& _reg, TWord& _value);
		static bool parseRegister_DDDDD(const std::string& _reg, TWord& _value);
		static bool parseRegister_JJJ(const std::string& _reg, bool _ab, TWord& _value);
		static bool parseRegister_QQQ(const std::string& _reg1, const std::string& _reg2, TWord& _value);
		static bool parseRegister_QQQQ(const std::string& _reg1, const std::string& _reg2, TWord& _value);
		static bool parseRegister_QQ(const std::string& _reg, TWord& _value);
		static bool parseRegister_qq(const std::string& _reg, TWord& _value);
		static bool parseRegister_JJ(const std::string& _reg, TWord& _value);
		static bool parseRegister_sss(const std::string& _reg, TWord& _value);
		static bool parseRegister_qqq(const std::string& _reg, TWord& _value);
		static bool parseRegister_ggg(const std::string& _reg, bool _D, TWord& _value);
		static bool parseRegister_EE(const std::string& _reg, TWord& _value);
		static bool parseRegister_LLL(const std::string& _reg, TWord& _value);
		static bool parseAluD(const std::string& _reg, TWord& _d);
		static bool parseRegisterR(const std::string& _reg, TWord& _r);
		static bool parseRegisterN(const std::string& _reg, TWord& _n);
		static bool parseRegisterM(const std::string& _reg, TWord& _m);
		static bool parseRegister_ee(const std::string& _reg, TWord& _value);
		static bool parseRegister_ff(const std::string& _reg, TWord& _value);
		static bool parseRegister_DDDD(const std::string& _reg, TWord& _value);
		static bool parseRegister_DD(const std::string& _reg, TWord& _value);

		// Value parsing
		static bool parseImmediate(const std::string& _text, TWord& _value);
		static bool parseNumber(const std::string& _text, TWord& _value);
		static bool parseConditionCode(const std::string& _cc, TWord& _value);
		static bool parseMemoryArea(const std::string& _area, EMemArea& _result);

		// Addressing mode parsing
		struct AddressingMode
		{
			TWord mmmrrr = 0;
			TWord extWord = 0;
			bool hasExtWord = false;
		};
		static bool parseAddressingMode(const std::string& _text, AddressingMode& _mode);

		// Field encoding
		static void setFieldValue(TWord& _word, Instruction _inst, Field _f, TWord _value);

		// Instruction assemblers
		AssembleResult assembleParallel(const std::string& _mnemonic, const std::string& _operands, const std::string& _moveStr) const;
		AssembleResult assembleNonParallel(const std::string& _mnemonic, const std::string& _operands) const;

		TWord encodeAlu(const std::string& _mnemonic, const std::string& _operands, bool& _success) const;
		TWord encodeParallelMove(const std::string& _moveStr, TWord& _extWord, bool& _hasExt, bool& _success) const;
		AssembleResult encodeMovexy(const std::string& _xMove, const std::string& _yMove) const;

		// Build mnemonic table
		void buildMnemonicTable();

		Opcodes m_opcodes;
		std::unordered_map<std::string, std::vector<Instruction>> m_mnemonics;
	};
}
