#include "assembler.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <sstream>
#include <map>

#include "disasm.h"
#include "opcodefields.h"
#include "opcodeinfo.h"
#include "opcodes.h"
#include "opcodetypes.h"
#include "registers.h"
#include "types.h"

namespace dsp56k
{
	// ============= forward declarations of disasm helpers =============
	extern const char* g_conditionCodes[16];
	extern const char* g_opNames[];

	// ============= Utility functions =============

	std::string Assembler::toLower(const std::string& _s)
	{
		std::string result = _s;
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return result;
	}

	Assembler::Tokens Assembler::tokenize(const std::string& _text)
	{
		Tokens tokens;
		std::string current;
		int parenDepth = 0;

		for (size_t i = 0; i < _text.size(); ++i)
		{
			const char c = _text[i];

			if (c == '(') ++parenDepth;
			if (c == ')') --parenDepth;

			if (parenDepth == 0 && (c == ' ' || c == '\t'))
			{
				if (!current.empty())
				{
					tokens.push_back(current);
					current.clear();
				}
			}
			else
			{
				current += c;
			}
		}
		if (!current.empty())
			tokens.push_back(current);

		return tokens;
	}

	bool Assembler::splitOperands(const std::string& _operands, std::vector<std::string>& _result)
	{
		_result.clear();
		if (_operands.empty())
			return true;

		std::string current;
		int parenDepth = 0;

		for (size_t i = 0; i < _operands.size(); ++i)
		{
			const char c = _operands[i];
			if (c == '(') ++parenDepth;
			if (c == ')') --parenDepth;

			if (c == ',' && parenDepth == 0)
			{
				_result.push_back(current);
				current.clear();
			}
			else
			{
				current += c;
			}
		}
		if (!current.empty())
			_result.push_back(current);

		return true;
	}

	// ============= Register encoding tables =============

	static const std::map<std::string, TWord>& getRegisterMap_dddddd()
	{
		static const std::map<std::string, TWord> m = {
			{"x0", 0x04}, {"x1", 0x05}, {"y0", 0x06}, {"y1", 0x07},
			{"a0", 0x08}, {"b0", 0x09}, {"a2", 0x0a}, {"b2", 0x0b},
			{"a1", 0x0c}, {"b1", 0x0d}, {"a", 0x0e}, {"b", 0x0f},
			{"r0", 0x10}, {"r1", 0x11}, {"r2", 0x12}, {"r3", 0x13},
			{"r4", 0x14}, {"r5", 0x15}, {"r6", 0x16}, {"r7", 0x17},
			{"n0", 0x18}, {"n1", 0x19}, {"n2", 0x1a}, {"n3", 0x1b},
			{"n4", 0x1c}, {"n5", 0x1d}, {"n6", 0x1e}, {"n7", 0x1f},
			{"m0", 0x20}, {"m1", 0x21}, {"m2", 0x22}, {"m3", 0x23},
			{"m4", 0x24}, {"m5", 0x25}, {"m6", 0x26}, {"m7", 0x27},
			{"ep", 0x2a},
			{"vba", 0x30}, {"sc", 0x31},
			{"sz", 0x38}, {"sr", 0x39}, {"omr", 0x3a}, {"sp", 0x3b},
			{"ssh", 0x3c}, {"ssl", 0x3d}, {"la", 0x3e}, {"lc", 0x3f}
		};
		return m;
	}

	static const std::map<std::string, TWord>& getRegisterMap_DDDDD()
	{
		static const std::map<std::string, TWord> m = {
			{"m0", 0x00}, {"m1", 0x01}, {"m2", 0x02}, {"m3", 0x03},
			{"m4", 0x04}, {"m5", 0x05}, {"m6", 0x06}, {"m7", 0x07},
			{"ep", 0x0a},
			{"vba", 0x10}, {"sc", 0x11},
			{"sz", 0x18}, {"sr", 0x19}, {"omr", 0x1a}, {"sp", 0x1b},
			{"ssh", 0x1c}, {"ssl", 0x1d}, {"la", 0x1e}, {"lc", 0x1f}
		};
		return m;
	}

	static const std::map<std::string, TWord>& getRegisterMap_DDDDDD()
	{
		// 5-bit register encoding used by Move_xx ddddd and Mover eeeee/ddddd fields
		static const std::map<std::string, TWord> m = {
			{"x0", 0x04}, {"x1", 0x05}, {"y0", 0x06}, {"y1", 0x07},
			{"a0", 0x08}, {"b0", 0x09}, {"a2", 0x0a}, {"b2", 0x0b},
			{"a1", 0x0c}, {"b1", 0x0d}, {"a", 0x0e}, {"b", 0x0f},
			{"r0", 0x10}, {"r1", 0x11}, {"r2", 0x12}, {"r3", 0x13},
			{"r4", 0x14}, {"r5", 0x15}, {"r6", 0x16}, {"r7", 0x17},
			{"n0", 0x18}, {"n1", 0x19}, {"n2", 0x1a}, {"n3", 0x1b},
			{"n4", 0x1c}, {"n5", 0x1d}, {"n6", 0x1e}, {"n7", 0x1f}
		};
		return m;
	}

	// ============= Register parsing functions =============

	bool Assembler::parseRegister_dddddd(const std::string& _reg, TWord& _value)
	{
		const auto& m = getRegisterMap_dddddd();
		const auto it = m.find(toLower(_reg));
		if (it == m.end()) return false;
		_value = it->second;
		return true;
	}

	bool Assembler::parseRegister_DDDDD(const std::string& _reg, TWord& _value)
	{
		const auto& m = getRegisterMap_DDDDD();
		const auto it = m.find(toLower(_reg));
		if (it == m.end()) return false;
		_value = it->second;
		return true;
	}

	bool Assembler::parseRegister_DDDD(const std::string& _reg, TWord& _value)
	{
		static const std::map<std::string, TWord> m = {
			{"x0", 4}, {"x1", 5}, {"y0", 6}, {"y1", 7},
			{"a0", 8}, {"b0", 9}, {"a2", 10}, {"b2", 11},
			{"a1", 12}, {"b1", 13}, {"a", 14}, {"b", 15}
		};
		const auto it = m.find(toLower(_reg));
		if (it == m.end()) return false;
		_value = it->second;
		return true;
	}

	bool Assembler::parseRegister_DD(const std::string& _reg, TWord& _value)
	{
		const auto& m = getRegisterMap_DDDDDD();
		const auto it = m.find(toLower(_reg));
		if (it == m.end()) return false;
		_value = it->second;
		return true;
	}

	bool Assembler::parseRegister_JJJ(const std::string& _reg, bool _ab, TWord& _value)
	{
		// _ab: true = destination is b, false = destination is a
		// JJJ=1 means "other accumulator" (for Add/Sub etc)
		// Note: for Tfr, JJJ=0 also falls through to mean "other accumulator" in the disassembler
		// The caller is responsible for handling the JJJ=0 case for Tfr
		const auto reg = toLower(_reg);

		if (reg == "b" && !_ab) { _value = 1; return true; }	// b when dest is a
		if (reg == "a" && _ab) { _value = 1; return true; }	// a when dest is b
		if (reg == "x")  { _value = 2; return true; }
		if (reg == "y")  { _value = 3; return true; }
		if (reg == "x0") { _value = 4; return true; }
		if (reg == "y0") { _value = 5; return true; }
		if (reg == "x1") { _value = 6; return true; }
		if (reg == "y1") { _value = 7; return true; }
		return false;
	}

	bool Assembler::parseRegister_QQQ(const std::string& _reg1, const std::string& _reg2, TWord& _value)
	{
		const auto r1 = toLower(_reg1);
		const auto r2 = toLower(_reg2);
		const auto pair = r1 + "," + r2;

		static const std::map<std::string, TWord> m = {
			{"x0,x0", 0}, {"y0,y0", 1}, {"x1,x0", 2}, {"y1,y0", 3},
			{"x0,y1", 4}, {"y0,x0", 5}, {"x1,y0", 6}, {"y1,x1", 7}
		};
		const auto it = m.find(pair);
		if (it == m.end()) return false;
		_value = it->second;
		return true;
	}

	bool Assembler::parseRegister_QQQQ(const std::string& _reg1, const std::string& _reg2, TWord& _value)
	{
		const auto pair = toLower(_reg1) + "," + toLower(_reg2);
		static const std::map<std::string, TWord> m = {
			{"x0,x0", 0},  {"y0,y0", 1},  {"x1,x0", 2},  {"y1,y0", 3},
			{"x0,y1", 4},  {"y0,x0", 5},  {"x1,y0", 6},  {"y1,x1", 7},
			{"x1,x1", 8},  {"y1,y1", 9},  {"x0,x1", 10}, {"y0,y1", 11},
			{"y1,x0", 12}, {"x0,y0", 13}, {"y0,x1", 14}, {"x1,y1", 15}
		};
		const auto it = m.find(pair);
		if (it == m.end()) return false;
		_value = it->second;
		return true;
	}

	bool Assembler::parseRegister_QQ(const std::string& _reg, TWord& _value)
	{
		// QQ: decode_QQ_read
		const auto reg = toLower(_reg);
		if (reg == "y1") { _value = 0; return true; }
		if (reg == "x0") { _value = 1; return true; }
		if (reg == "y0") { _value = 2; return true; }
		if (reg == "x1") { _value = 3; return true; }
		return false;
	}

	bool Assembler::parseRegister_qq(const std::string& _reg, TWord& _value)
	{
		// qq: decode_qq_read
		const auto reg = toLower(_reg);
		if (reg == "x0") { _value = 0; return true; }
		if (reg == "y0") { _value = 1; return true; }
		if (reg == "x1") { _value = 2; return true; }
		if (reg == "y1") { _value = 3; return true; }
		return false;
	}

	bool Assembler::parseRegister_JJ(const std::string& _reg, TWord& _value)
	{
		// JJ: decode_JJ_read
		const auto reg = toLower(_reg);
		if (reg == "x0") { _value = 0; return true; }
		if (reg == "y0") { _value = 1; return true; }
		if (reg == "x1") { _value = 2; return true; }
		if (reg == "y1") { _value = 3; return true; }
		return false;
	}

	bool Assembler::parseRegister_sss(const std::string& _reg, TWord& _value)
	{
		const auto reg = toLower(_reg);
		if (reg == "a1") { _value = 2; return true; }
		if (reg == "b1") { _value = 3; return true; }
		if (reg == "x0") { _value = 4; return true; }
		if (reg == "y0") { _value = 5; return true; }
		if (reg == "x1") { _value = 6; return true; }
		if (reg == "y1") { _value = 7; return true; }
		return false;
	}

	bool Assembler::parseRegister_qqq(const std::string& _reg, TWord& _value)
	{
		const auto reg = toLower(_reg);
		if (reg == "a0") { _value = 2; return true; }
		if (reg == "b0") { _value = 3; return true; }
		if (reg == "x0") { _value = 4; return true; }
		if (reg == "y0") { _value = 5; return true; }
		if (reg == "x1") { _value = 6; return true; }
		if (reg == "y1") { _value = 7; return true; }
		return false;
	}

	bool Assembler::parseRegister_ggg(const std::string& _reg, bool _D, TWord& _value)
	{
		const auto reg = toLower(_reg);
		// ggg=0 is "other accumulator" (opposite of D)
		if (!_D && reg == "b") { _value = 0; return true; }
		if (_D && reg == "a") { _value = 0; return true; }
		if (reg == "x0") { _value = 4; return true; }
		if (reg == "y0") { _value = 5; return true; }
		if (reg == "x1") { _value = 6; return true; }
		if (reg == "y1") { _value = 7; return true; }
		return false;
	}

	bool Assembler::parseRegister_EE(const std::string& _reg, TWord& _value)
	{
		const auto reg = toLower(_reg);
		if (reg == "mr")  { _value = 0; return true; }
		if (reg == "ccr") { _value = 1; return true; }
		if (reg == "omr") { _value = 2; return true; }
		if (reg == "eom") { _value = 3; return true; }
		return false;
	}

	bool Assembler::parseRegister_LLL(const std::string& _reg, TWord& _value)
	{
		const auto reg = toLower(_reg);
		if (reg == "a10") { _value = 0; return true; }
		if (reg == "b10") { _value = 1; return true; }
		if (reg == "x")   { _value = 2; return true; }
		if (reg == "y")   { _value = 3; return true; }
		if (reg == "a")   { _value = 4; return true; }
		if (reg == "b")   { _value = 5; return true; }
		if (reg == "ab")  { _value = 6; return true; }
		if (reg == "ba")  { _value = 7; return true; }
		return false;
	}

	bool Assembler::parseAluD(const std::string& _reg, TWord& _d)
	{
		const auto reg = toLower(_reg);
		if (reg == "a") { _d = 0; return true; }
		if (reg == "b") { _d = 1; return true; }
		return false;
	}

	bool Assembler::parseRegisterR(const std::string& _reg, TWord& _r)
	{
		const auto reg = toLower(_reg);
		if (reg.size() == 2 && reg[0] == 'r' && reg[1] >= '0' && reg[1] <= '7')
		{
			_r = reg[1] - '0';
			return true;
		}
		return false;
	}

	bool Assembler::parseRegisterN(const std::string& _reg, TWord& _n)
	{
		const auto reg = toLower(_reg);
		if (reg.size() == 2 && reg[0] == 'n' && reg[1] >= '0' && reg[1] <= '7')
		{
			_n = reg[1] - '0';
			return true;
		}
		return false;
	}

	bool Assembler::parseRegisterM(const std::string& _reg, TWord& _m)
	{
		const auto reg = toLower(_reg);
		if (reg.size() == 2 && reg[0] == 'm' && reg[1] >= '0' && reg[1] <= '7')
		{
			_m = reg[1] - '0';
			return true;
		}
		return false;
	}

	bool Assembler::parseRegister_ee(const std::string& _reg, TWord& _value)
	{
		const auto reg = toLower(_reg);
		if (reg == "x0") { _value = 0; return true; }
		if (reg == "x1") { _value = 1; return true; }
		if (reg == "a")  { _value = 2; return true; }
		if (reg == "b")  { _value = 3; return true; }
		return false;
	}

	bool Assembler::parseRegister_ff(const std::string& _reg, TWord& _value)
	{
		const auto reg = toLower(_reg);
		if (reg == "y0") { _value = 0; return true; }
		if (reg == "y1") { _value = 1; return true; }
		if (reg == "a")  { _value = 2; return true; }
		if (reg == "b")  { _value = 3; return true; }
		return false;
	}

	// ============= Value parsing =============

	bool Assembler::parseImmediate(const std::string& _text, TWord& _value)
	{
		auto text = toLower(_text);

		// Strip # prefix
		if (!text.empty() && text[0] == '#')
			text = text.substr(1);

		// Strip < or > size prefix
		if (!text.empty() && (text[0] == '<' || text[0] == '>'))
			text = text.substr(1);

		return parseNumber(text, _value);
	}

	bool Assembler::parseNumber(const std::string& _text, TWord& _value)
	{
		if (_text.empty())
			return false;

		auto text = _text;

		// Disassembler labels: int_XXXXXX (PC-relative) or func_XXXXXX (absolute)
		if (text.size() > 4 && text.substr(0, 4) == "int_")
		{
			text = "$" + text.substr(4);
		}
		else if (text.size() > 5 && text.substr(0, 5) == "func_")
		{
			text = "$" + text.substr(5);
		}

		// Hex with $ prefix
		if (text[0] == '$')
		{
			text = text.substr(1);
			if (text.empty()) return false;
			char* end = nullptr;
			const unsigned long val = strtoul(text.c_str(), &end, 16);
			if (end != text.c_str() + text.size()) return false;
			_value = static_cast<TWord>(val);
			return true;
		}

		// Hex with 0x prefix
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		{
			text = text.substr(2);
			char* end = nullptr;
			const unsigned long val = strtoul(text.c_str(), &end, 16);
			if (end != text.c_str() + text.size()) return false;
			_value = static_cast<TWord>(val);
			return true;
		}

		// Decimal
		{
			char* end = nullptr;
			const unsigned long val = strtoul(text.c_str(), &end, 10);
			if (end != text.c_str() + text.size()) return false;
			_value = static_cast<TWord>(val);
			return true;
		}
	}

	bool Assembler::parseConditionCode(const std::string& _cc, TWord& _value)
	{
		const auto cc = toLower(_cc);
		for (TWord i = 0; i < 16; ++i)
		{
			if (cc == g_conditionCodes[i])
			{
				_value = i;
				return true;
			}
		}
		return false;
	}

	bool Assembler::parseMemoryArea(const std::string& _area, EMemArea& _result)
	{
		const auto a = toLower(_area);
		if (a == "x") { _result = MemArea_X; return true; }
		if (a == "y") { _result = MemArea_Y; return true; }
		if (a == "p") { _result = MemArea_P; return true; }
		return false;
	}

	// ============= Addressing mode parsing =============

	bool Assembler::parseAddressingMode(const std::string& _text, AddressingMode& _mode)
	{
		auto text = toLower(_text);
		_mode = {};

		// -(rN) - predecrement
		if (text.size() >= 4 && text[0] == '-' && text[1] == '(')
		{
			TWord r;
			auto inner = text.substr(2, text.size() - 3); // strip "-(" and ")"
			if (!parseRegisterR(inner, r))
				return false;
			_mode.mmmrrr = MMMRRR_MinusRn | r;
			return true;
		}

		// (rN...) forms
		if (text.size() >= 4 && text[0] == '(')
		{
			auto closeParen = text.find(')');
			if (closeParen == std::string::npos)
				return false;

			const auto inner = text.substr(1, closeParen - 1);
			const auto suffix = text.substr(closeParen + 1);

			// (rN+nN) - indexed no update
			auto plusN = inner.find('+');
			if (plusN != std::string::npos)
			{
				auto regPart = inner.substr(0, plusN);
				auto nPart = inner.substr(plusN + 1);
				TWord r, n;
				if (parseRegisterR(regPart, r) && parseRegisterN(nPart, n) && r == n && suffix.empty())
				{
					_mode.mmmrrr = MMMRRR_RnPlusNnNoUpdate | r;
					return true;
				}
			}

			TWord r;
			if (!parseRegisterR(inner, r))
				return false;

			if (suffix.empty())
			{
				_mode.mmmrrr = MMMRRR_Rn | r;
				return true;
			}
			if (suffix == "+")
			{
				_mode.mmmrrr = MMMRRR_RnPlus | r;
				return true;
			}
			if (suffix == "-")
			{
				_mode.mmmrrr = MMMRRR_RnMinus | r;
				return true;
			}
			// (rN)+nN or (rN)-nN
			if (suffix.size() >= 3)
			{
				const char op = suffix[0];
				TWord n;
				if (parseRegisterN(suffix.substr(1), n) && r == n)
				{
					if (op == '+') { _mode.mmmrrr = MMMRRR_RnPlusNn | r; return true; }
					if (op == '-') { _mode.mmmrrr = MMMRRR_RnMinusNn | r; return true; }
				}
			}
		}

		return false;
	}

	// ============= Field encoding =============

	void Assembler::setFieldValue(TWord& _word, Instruction _inst, Field _f, TWord _value)
	{
		const auto& fi = getFieldInfo(_inst, _f);
		if (fi.len == 0)
			return;
		_word = (_word & ~(fi.mask << fi.bit)) | ((_value & fi.mask) << fi.bit);
	}

	// ============= Mnemonic table builder =============

	void Assembler::buildMnemonicTable()
	{
		for (int i = 0; i < InstructionCount; ++i)
		{
			const auto inst = static_cast<Instruction>(i);
			if (inst == ResolveCache || inst == Parallel)
				continue;

			std::string name = toLower(g_opNames[i]);

			// Special handling for instructions with condition codes in the name
			// b/bs/j/js prefixes + cc are handled by appending cc during lookup
			// Ifcc has empty name, handled separately

			if (!name.empty())
				m_mnemonics[name].push_back(inst);
		}

		// Additional mappings for macsu/macuu/mpysu/mpyuu (suffix appended by disassembler)
		m_mnemonics["macsu"].push_back(Macsu);
		m_mnemonics["macuu"].push_back(Macsu);
		m_mnemonics["mpysu"].push_back(Mpy_su);
		m_mnemonics["mpyuu"].push_back(Mpy_su);
	}

	// ============= Constructor =============

	Assembler::Assembler()
	{
		buildMnemonicTable();
	}

	// ============= Symbol management =============

	void Assembler::addSymbol(SymbolType _type, TWord _address, const std::string& _name)
	{
		m_symbols[_type][toLower(_name)] = _address;
	}

	void Assembler::addBitSymbol(SymbolType _type, TWord _address, TWord _bit, const std::string& _name)
	{
		m_bitSymbols[_type][_address].bits[toLower(_name)] = _bit;
	}

	void Assembler::addBitMaskSymbol(SymbolType _type, TWord _address, TWord _bitMask, const std::string& _name)
	{
		// Find the bit position from the mask (find lowest set bit)
		for (TWord b = 0; b < 24; ++b)
		{
			if (_bitMask == (1u << b))
			{
				addBitSymbol(_type, _address, b, _name);
				return;
			}
		}
		// If it's a multi-bit mask, store it as-is with a special encoding
		m_bitSymbols[_type][_address].bits[toLower(_name)] = _bitMask;
	}

	void Assembler::importSymbolsFromDisassembler(const Disassembler& _disasm)
	{
		static constexpr std::pair<Disassembler::SymbolType, SymbolType> typeMap[] =
		{
			{Disassembler::MemX, MemX},
			{Disassembler::MemY, MemY},
			{Disassembler::MemP, MemP},
			{Disassembler::MemL, MemL},
		};

		for (const auto& [disasmType, asmType] : typeMap)
		{
			const auto& symbols = _disasm.getSymbols(disasmType);
			for (const auto& [address, name] : symbols)
				addSymbol(asmType, address, name);
		}
	}

	bool Assembler::resolveSymbol(EMemArea _area, const std::string& _name, TWord& _address) const
	{
		SymbolType type;
		switch (_area)
		{
		case MemArea_X: type = MemX; break;
		case MemArea_Y: type = MemY; break;
		case MemArea_P: type = MemP; break;
		default: return false;
		}

		const auto it = m_symbols[type].find(toLower(_name));
		if (it == m_symbols[type].end())
			return false;
		_address = it->second;
		return true;
	}

	bool Assembler::resolveBitSymbol(EMemArea _area, TWord _peripheralAddress, const std::string& _name, TWord& _bit) const
	{
		SymbolType type;
		switch (_area)
		{
		case MemArea_X: type = MemX; break;
		case MemArea_Y: type = MemY; break;
		case MemArea_P: type = MemP; break;
		default: return false;
		}

		const auto itAddr = m_bitSymbols[type].find(_peripheralAddress);
		if (itAddr == m_bitSymbols[type].end())
			return false;

		const auto itBit = itAddr->second.bits.find(toLower(_name));
		if (itBit == itAddr->second.bits.end())
			return false;
		_bit = itBit->second;
		return true;
	}

	// ============= ALU encoding (parallel instructions, lower 8 bits) =============

	TWord Assembler::encodeAlu(const std::string& _mnemonic, const std::string& _operands, bool& _success) const
	{
		_success = false;
		const auto mnem = toLower(_mnemonic);
		std::vector<std::string> ops;
		splitOperands(_operands, ops);

		// Find matching parallel ALU instruction
		auto it = m_mnemonics.find(mnem);
		if (it == m_mnemonics.end())
			return 0;

		for (const auto inst : it->second)
		{
			const auto& oi = g_opcodes[inst];

			// Only consider parallel ALU instructions (those with MoveOperation field = '?' upper 16 bits)
			if (!hasField(oi, Field_MoveOperation))
				continue;

			TWord word = oi.m_mask1; // start with fixed '1' bits

			switch (inst)
			{
			// Single destination ALU (D only)
			case Abs: case Asl_D: case Asr_D: case Clr: case Lsl_D: case Lsr_D:
			case Neg: case Not: case Rnd: case Rol: case Ror: case Tst:
			{
				if (ops.size() != 1) continue;
				TWord d;
				if (!parseAluD(ops[0], d)) continue;
				setFieldValue(word, inst, Field_d, d);
				setFieldValue(word, inst, Field_D, d);
				_success = true;
				return word & 0xFF;
			}

			// Max/Maxm: always "a,b"
			case Max: case Maxm:
			{
				if (ops.size() != 2) continue;
				if (toLower(ops[0]) != "a" || toLower(ops[1]) != "b") continue;
				_success = true;
				return word & 0xFF;
			}

			// Source,Dest ALU (JJJ,d)
			case Add_SD: case Sub_SD: case Cmp_S1S2: case Cmpm_S1S2:
			case Tfr:
			{
				if (ops.size() == 1)
				{
					// Some instructions like addl/addr take only D
					continue;
				}
				if (ops.size() != 2) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				TWord jjj;
				if (!parseRegister_JJJ(ops[0], d != 0, jjj)) continue;

				// For Tfr, Cmp_S1S2, Cmpm_S1S2: JJJ value 0 is not valid
				if ((inst == Tfr || inst == Cmp_S1S2 || inst == Cmpm_S1S2) && jjj == 0)
					continue;

				setFieldValue(word, inst, Field_JJJ, jjj);
				setFieldValue(word, inst, Field_d, d);
				_success = true;
				return word & 0xFF;
			}

			// Source,Dest ALU (JJ,d) for And/Or/Eor
			case And_SD: case Or_SD: case Eor_SD:
			{
				if (ops.size() != 2) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				TWord jj;
				if (!parseRegister_JJ(ops[0], jj)) continue;
				setFieldValue(word, inst, Field_JJ, jj);
				setFieldValue(word, inst, Field_d, d);
				_success = true;
				return word & 0xFF;
			}

			// ADC/SBC: J,D
			case ADC: case Sbc:
			{
				if (ops.size() != 2) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				const auto src = toLower(ops[0]);
				TWord j;
				if (src == "x") j = 0;
				else if (src == "y") j = 1;
				else continue;
				setFieldValue(word, inst, Field_J, j);
				setFieldValue(word, inst, Field_d, d);
				_success = true;
				return word & 0xFF;
			}

			// Addl, Addr, Subl, Subr: S,D (implied source is other accumulator)
			case Addl: case Addr: case Subl: case Subr:
			{
				if (ops.size() != 2) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				// Source is always the other accumulator
				const auto src = toLower(ops[0]);
				if (d == 0 && src != "b") continue;
				if (d == 1 && src != "a") continue;
				setFieldValue(word, inst, Field_d, d);
				_success = true;
				return word & 0xFF;
			}

			// Multiply operations: QQQ,d with optional negate
			case Mpy_S1S2D: case Mpyr_S1S2D: case Mac_S1S2: case Macr_S1S2:
			{
				if (ops.size() != 3) continue;

				bool negate = false;
				auto first = ops[0];
				if (!first.empty() && first[0] == '-')
				{
					negate = true;
					first = first.substr(1);
				}

				TWord d;
				if (!parseAluD(ops[2], d)) continue;
				TWord qqq;
				if (!parseRegister_QQQ(first, ops[1], qqq)) continue;
				setFieldValue(word, inst, Field_QQQ, qqq);
				setFieldValue(word, inst, Field_d, d);
				setFieldValue(word, inst, Field_k, negate ? 1 : 0);
				_success = true;
				return word & 0xFF;
			}

			default:
				break;
			}
		}

		return 0;
	}

	// ============= Parallel move encoding (upper 16 bits) =============

	TWord Assembler::encodeParallelMove(const std::string& _moveStr, TWord& _extWord, bool& _hasExt, bool& _success) const
	{
		_success = false;
		_hasExt = false;

		if (_moveStr.empty())
		{
			// No move = Move_Nop
			_success = true;
			return g_opcodes[Move_Nop].m_mask1 & 0xFFFF00;
		}

		const auto moveStr = toLower(_moveStr);

		// Parse the move operands
		std::vector<std::string> ops;
		splitOperands(moveStr, ops);

		if (ops.size() == 1)
		{
			// Bare addressing mode = Move_ea: e.g. "(r0)+n0", "(r0)+", "(r0)-"
			AddressingMode am;
			if (parseAddressingMode(ops[0], am))
			{
				TWord word = g_opcodes[Move_ea].m_mask1;
				setFieldValue(word, Move_ea, Field_MM, am.mmmrrr >> 3);
				setFieldValue(word, Move_ea, Field_RRR, am.mmmrrr & 7);
				if (am.hasExtWord)
				{
					_extWord = am.extWord;
					_hasExt = true;
				}
				_success = true;
				return word & 0xFFFF00;
			}
			return 0;
		}

		if (ops.size() != 2)
			return 0;

		// Determine move type based on operands
		const auto& src = ops[0];
		const auto& dst = ops[1];

		// Check for immediate move: #xx,D
		if (!src.empty() && src[0] == '#')
		{
			TWord imm;
			if (!parseImmediate(src, imm))
				return 0;

			// Check if it's a short immediate (#<) for Move_xx
			bool isShort = (src.size() > 1 && src[1] == '<') || imm <= 0xFF;
			bool isLong = (src.size() > 1 && src[1] == '>');

			if (!isLong && isShort && imm <= 0xFF)
			{
				// Move_xx: immediate short data move to 5-bit register
				TWord dd;
				const auto& regMap = getRegisterMap_DDDDDD();
				auto it = regMap.find(dst);
				if (it == regMap.end()) return 0;
				dd = it->second;

				TWord word = g_opcodes[Move_xx].m_mask1;
				setFieldValue(word, Move_xx, Field_ddddd, dd);
				setFieldValue(word, Move_xx, Field_iiiiiiii, imm);
				_success = true;
				return word & 0xFFFF00;
			}
			else
			{
				// Long immediate → need to use Movex_ea with MMMRRR_ImmediateData
				// This is encoded as a X: memory move with effective address = #xxxx
				TWord dd;
				if (!parseRegister_dddddd(dst, dd)) return 0;
				// Only allow dd values that fit in the dd/ddd fields (5-bit encoding)
				if (dd > 0x1f) return 0;

				TWord word = g_opcodes[Movex_ea].m_mask1;
				setFieldValue(word, Movex_ea, Field_dd, dd >> 3);
				setFieldValue(word, Movex_ea, Field_ddd, dd & 0x7);
				setFieldValue(word, Movex_ea, Field_W, 1); // read into register
				setFieldValue(word, Movex_ea, Field_MMM, 6); // special mode
				setFieldValue(word, Movex_ea, Field_RRR, 4); // immediate data
				_extWord = imm;
				_hasExt = true;
				_success = true;
				return word & 0xFFFF00;
			}
		}

		// Check for memory area prefix: x:...,D or S,x:...
		auto hasMemPrefix = [](const std::string& s) -> bool {
			return s.size() >= 2 && (s[0] == 'x' || s[0] == 'y' || s[0] == 'l') && s[1] == ':';
		};

		bool srcIsMem = hasMemPrefix(src);
		bool dstIsMem = hasMemPrefix(dst);

		if (srcIsMem || dstIsMem)
		{
			const auto& memOp = srcIsMem ? src : dst;
			const auto& regOp = srcIsMem ? dst : src;
			bool write = srcIsMem; // write = reading from memory into register

			const char area = memOp[0];
			auto addrStr = memOp.substr(2); // strip "x:" or "y:"

			if (area == 'l')
			{
				// Long move: Movel_ea or Movel_aa
				TWord lll;
				if (!parseRegister_LLL(regOp, lll))
					return 0;

				AddressingMode am;
				if (parseAddressingMode(addrStr, am))
				{
					TWord word = g_opcodes[Movel_ea].m_mask1;
					setFieldValue(word, Movel_ea, Field_L, lll >> 2);
					setFieldValue(word, Movel_ea, Field_LL, lll & 0x3);
					setFieldValue(word, Movel_ea, Field_W, write ? 1 : 0);
					setFieldValue(word, Movel_ea, Field_MMM, am.mmmrrr >> 3);
					setFieldValue(word, Movel_ea, Field_RRR, am.mmmrrr & 7);
					if (am.hasExtWord)
					{
						_extWord = am.extWord;
						_hasExt = true;
					}
					_success = true;
					return word & 0xFFFF00;
				}

				// Try absolute short address
				if (addrStr[0] == '<')
				{
					TWord addr;
					if (!parseNumber(addrStr.substr(1), addr)) return 0;
					TWord word = g_opcodes[Movel_aa].m_mask1;
					setFieldValue(word, Movel_aa, Field_L, lll >> 2);
					setFieldValue(word, Movel_aa, Field_LL, lll & 0x3);
					setFieldValue(word, Movel_aa, Field_W, write ? 1 : 0);
					setFieldValue(word, Movel_aa, Field_aaaaaa, addr);
					_success = true;
					return word & 0xFFFF00;
				}

				return 0;
			}

			// X or Y memory moves
			const bool isY = (area == 'y');
			const auto moveInst = isY ? Movey_ea : Movex_ea;
			const auto moveInstAA = isY ? Movey_aa : Movex_aa;

			// Try effective address
			AddressingMode am;
			if (parseAddressingMode(addrStr, am))
			{
				TWord dd;
				if (!parseRegister_dddddd(regOp, dd)) return 0;
				if (dd > 0x1f) return 0; // dd+ddd is 5 bits

				TWord word = g_opcodes[moveInst].m_mask1;
				setFieldValue(word, moveInst, Field_dd, dd >> 3);
				setFieldValue(word, moveInst, Field_ddd, dd & 0x7);
				setFieldValue(word, moveInst, Field_W, write ? 1 : 0);
				setFieldValue(word, moveInst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, moveInst, Field_RRR, am.mmmrrr & 7);
				if (am.hasExtWord)
				{
					_extWord = am.extWord;
					_hasExt = true;
				}
				_success = true;
				return word & 0xFFFF00;
			}

			// Try absolute address: >$xxxx (long, extension word) or $xx (short, inline)
			bool isAbsShort = false;
			TWord addr;
			if (!addrStr.empty() && addrStr[0] == '>')
			{
				// Long absolute → use ea with AbsAddr
				if (!parseNumber(addrStr.substr(1), addr)) return 0;
				TWord dd;
				if (!parseRegister_dddddd(regOp, dd)) return 0;
				if (dd > 0x1f) return 0;

				TWord word = g_opcodes[moveInst].m_mask1;
				setFieldValue(word, moveInst, Field_dd, dd >> 3);
				setFieldValue(word, moveInst, Field_ddd, dd & 0x7);
				setFieldValue(word, moveInst, Field_W, write ? 1 : 0);
				setFieldValue(word, moveInst, Field_MMM, 6); // absolute address
				setFieldValue(word, moveInst, Field_RRR, 0);
				_extWord = addr;
				_hasExt = true;
				_success = true;
				return word & 0xFFFF00;
			}

			// Absolute short
			if (parseNumber(addrStr.size() > 0 && addrStr[0] == '<' ? addrStr.substr(1) : addrStr, addr))
			{
				if (addr <= 0x3f)
				{
					TWord dd;
					if (!parseRegister_dddddd(regOp, dd)) return 0;
					if (dd > 0x1f) return 0;

					TWord word = g_opcodes[moveInstAA].m_mask1;
					setFieldValue(word, moveInstAA, Field_dd, dd >> 3);
					setFieldValue(word, moveInstAA, Field_ddd, dd & 0x7);
					setFieldValue(word, moveInstAA, Field_W, write ? 1 : 0);
					setFieldValue(word, moveInstAA, Field_aaaaaa, addr);
					_success = true;
					return word & 0xFFFF00;
				}
			}

			return 0;
		}

		// Register to register move (Mover): S,D
		{
			const auto& regMap = getRegisterMap_DDDDDD();
			auto itSrc = regMap.find(src);
			auto itDst = regMap.find(dst);
			if (itSrc != regMap.end() && itDst != regMap.end())
			{
				TWord word = g_opcodes[Mover].m_mask1;
				setFieldValue(word, Mover, Field_eeeee, itSrc->second);
				setFieldValue(word, Mover, Field_ddddd, itDst->second);
				_success = true;
				return word & 0xFFFF00;
			}
		}

		return 0;
	}

	// ============= Movexy encoding (X:ea Y:ea parallel move) =============

	AssembleResult Assembler::encodeMovexy(const std::string& _xMove, const std::string& _yMove) const
	{
		AssembleResult result;

		// Parse X move: "reg,x:ea" or "x:ea,reg"
		std::vector<std::string> xOps, yOps;
		splitOperands(_xMove, xOps);
		splitOperands(_yMove, yOps);

		if (xOps.size() != 2 || yOps.size() != 2) return result;

		// Determine direction for X: is x: on src or dst side?
		bool writeX = false; // false = store reg to X:ea, true = load X:ea to reg (W=1)
		std::string xReg, xEa;
		auto xop0 = toLower(xOps[0]);
		auto xop1 = toLower(xOps[1]);

		if (xop0.size() > 2 && xop0[0] == 'x' && xop0[1] == ':') {
			writeX = true; xEa = xop0.substr(2); xReg = xop1;
		} else if (xop1.size() > 2 && xop1[0] == 'x' && xop1[1] == ':') {
			writeX = false; xEa = xop1.substr(2); xReg = xop0;
		} else return result;

		// Determine direction for Y
		bool writeY = false;
		std::string yReg, yEa;
		auto yop0 = toLower(yOps[0]);
		auto yop1 = toLower(yOps[1]);

		if (yop0.size() > 2 && yop0[0] == 'y' && yop0[1] == ':') {
			writeY = true; yEa = yop0.substr(2); yReg = yop1;
		} else if (yop1.size() > 2 && yop1[0] == 'y' && yop1[1] == ':') {
			writeY = false; yEa = yop1.substr(2); yReg = yop0;
		} else return result;

		// Parse X register (ee encoding): x0=0, x1=1, a=2, b=3
		TWord ee;
		if (!parseRegister_ee(xReg, ee)) return result;

		// Parse Y register (ff encoding): y0=0, y1=1, a=2, b=3
		TWord ff;
		if (!parseRegister_ff(yReg, ff)) return result;

		// Parse X effective address
		AddressingMode xAm;
		if (!parseAddressingMode(xEa, xAm)) return result;

		// Parse Y effective address
		AddressingMode yAm;
		if (!parseAddressingMode(yEa, yAm)) return result;

		// X EA gives MM,RRR directly (5 bits)
		TWord mrX = xAm.mmmrrr;
		// But Movexy only supports modes 0-5,7 (not mode 6 = absolute), and the M field is 2 bits
		TWord mmmX = (mrX >> 3) & 0x7;
		TWord rrrX = mrX & 0x7;
		if (mmmX == 6) return result; // absolute address not supported in Movexy

		// For the Y EA: the Y register number (rr) is adjusted relative to X RRR
		// Disassembler: rrY += (rrrX >= 4) ? 0 : 4
		// So to reverse: actual Y rrrY - offset = rr field value
		TWord mmmY = (yAm.mmmrrr >> 3) & 0x7;
		TWord rrrY = yAm.mmmrrr & 0x7;
		if (mmmY == 6) return result; // absolute not supported

		TWord rrField;
		if (rrrX >= 4)
			rrField = rrrY;       // no offset was added
		else
			rrField = rrrY - 4;   // offset of 4 was added, remove it

		if (rrField > 3) return result; // rr is only 2 bits

		// Encode the Movexy word
		// Pattern: 1wmmeeffWrrMMRRR????????
		TWord word = g_opcodes[Movexy].m_mask1;
		setFieldValue(word, Movexy, Field_W, writeX ? 1 : 0);
		setFieldValue(word, Movexy, Field_w, writeY ? 1 : 0);
		setFieldValue(word, Movexy, Field_ee, ee);
		setFieldValue(word, Movexy, Field_ff, ff);
		setFieldValue(word, Movexy, Field_MM, mmmX & 0x3); // MM is 2 bits (modes 0-5,7 map to 2 bits)
		setFieldValue(word, Movexy, Field_RRR, rrrX);
		setFieldValue(word, Movexy, Field_mm, mmmY & 0x3);
		setFieldValue(word, Movexy, Field_rr, rrField);

		result.word[0] = word & 0xFFFF00; // clear lower 8 bits for ALU
		result.wordCount = 1;
		result.error = AssembleError::OK;
		return result;
	}

	// ============= Non-parallel instruction encoding =============

	AssembleResult Assembler::assembleNonParallel(const std::string& _mnemonic, const std::string& _operands) const
	{
		AssembleResult result;
		const auto mnem = toLower(_mnemonic);

		std::vector<std::string> ops;
		splitOperands(_operands, ops);

		// Check all instruction variants for this mnemonic
		auto it = m_mnemonics.find(mnem);
		if (it == m_mnemonics.end())
			return result;

		for (const auto inst : it->second)
		{
			const auto& oi = g_opcodes[inst];

			// Only consider non-parallel instructions
			if (!isNonParallelOpcode(oi))
				continue;

			TWord word = oi.m_mask1;

			switch (inst)
			{
			// No-operand instructions
			case Debug: case Enddo: case Illegal: case Nop: case Pflush: case Pflushun:
			case Pfree: case Reset: case Rti: case Rts: case Stop: case Trap: case Wait:
			{
				if (ops.size() != 0 && !(ops.size() == 1 && ops[0].empty())) continue;
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Inc/Dec: d
			case Inc: case Dec:
			{
				if (ops.size() != 1) continue;
				TWord d;
				if (!parseAluD(ops[0], d)) continue;
				setFieldValue(word, inst, Field_d, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Short immediate ALU: #<xx,D
			case Add_xx: case And_xx: case Cmp_xxS2: case Eor_xx: case Or_xx: case Sub_xx:
			{
				if (ops.size() != 2) continue;
				if (ops[0].empty() || ops[0][0] != '#') continue;
				// Must be short immediate (#< prefix or small value), not explicit long (#>)
				auto immStr = ops[0];
				bool explicitShort = (immStr.size() > 1 && immStr[1] == '<');
				bool explicitLong = (immStr.size() > 1 && immStr[1] == '>');
				if (explicitLong) continue;
				TWord imm;
				if (!parseImmediate(immStr, imm)) continue;
				if (!explicitShort && imm > 0x3F) continue; // 6-bit immediate
				if (imm > 0x3F) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_iiiiii, imm);
				setFieldValue(word, inst, Field_d, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Long immediate ALU: #>xxxx,D
			case Add_xxxx: case And_xxxx: case Cmp_xxxxS2: case Eor_xxxx: case Or_xxxx: case Sub_xxxx:
			{
				if (ops.size() != 2) continue;
				if (ops[0].empty() || ops[0][0] != '#') continue;
				auto immStr = ops[0];
				bool explicitLong = (immStr.size() > 1 && immStr[1] == '>');
				TWord imm;
				if (!parseImmediate(immStr, imm)) continue;
				if (!explicitLong) continue; // must be explicit long
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_d, d);
				result.word[0] = word;
				result.word[1] = imm;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// ORI/ANDI: #xx,EE
			case Ori: case Andi:
			{
				if (ops.size() != 2) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0xFF) continue;
				TWord ee;
				if (!parseRegister_EE(ops[1], ee)) continue;
				setFieldValue(word, inst, Field_iiiiiiii, imm);
				setFieldValue(word, inst, Field_EE, ee);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// ASL/ASR with immediate shift: #ii,S,D
			case Asl_ii: case Asr_ii:
			{
				if (ops.size() != 3) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0x3F) continue;
				TWord s, d;
				if (!parseAluD(ops[1], s)) continue;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_iiiiii, imm);
				setFieldValue(word, inst, Field_S, s);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// ASL/ASR with register shift: S1,S2,D
			case Asl_S1S2D: case Asr_S1S2D:
			{
				if (ops.size() != 3) continue;
				TWord sss;
				if (!parseRegister_sss(ops[0], sss)) continue;
				TWord s, d;
				if (!parseAluD(ops[1], s)) continue;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_sss, sss);
				setFieldValue(word, inst, Field_S, s);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// LSL/LSR with immediate: #ii,D
			case Lsl_ii: case Lsr_ii:
			{
				if (ops.size() != 2) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0x1F) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_iiiii, imm);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// LSL/LSR with register: S,D
			case Lsl_SD: case Lsr_SD:
			{
				if (ops.size() != 2) continue;
				TWord sss;
				if (!parseRegister_sss(ops[0], sss)) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_sss, sss);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// DIV: S,D (JJ register, d accumulator)
			case Div:
			{
				if (ops.size() != 2) continue;
				TWord jj;
				if (!parseRegister_JJ(ops[0], jj)) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_JJ, jj);
				setFieldValue(word, inst, Field_d, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// DMAC: ss/su/us/uu [-]QQQQ,D
			case Dmac:
			{
				// Operands string is "ss x0,x0,a" or "su -y1,x0,b"
				// First split by space to get sign mode and QQQQ,D
				auto spacePos = _operands.find(' ');
				if (spacePos == std::string::npos) continue;

				auto signMode = toLower(_operands.substr(0, spacePos));
				auto rest = _operands.substr(spacePos + 1);

				// Parse sign mode
				TWord ss;
				if (signMode == "ss") ss = 0;
				else if (signMode == "su") ss = 1;
				else if (signMode == "us") ss = 2;
				else if (signMode == "uu") ss = 3;
				else continue;

				// Check for negate prefix
				bool negate = false;
				if (!rest.empty() && rest[0] == '-')
				{
					negate = true;
					rest = rest.substr(1);
				}

				// Split rest into operands: "x0,x0,a" -> ["x0", "x0", "a"]
				std::vector<std::string> dmacOps;
				splitOperands(rest, dmacOps);
				if (dmacOps.size() != 3) continue;

				TWord qqqq;
				if (!parseRegister_QQQQ(dmacOps[0], dmacOps[1], qqqq)) continue;
				TWord d;
				if (!parseAluD(dmacOps[2], d)) continue;

				setFieldValue(word, inst, Field_S, (ss >> 1) & 1);
				setFieldValue(word, inst, Field_s, ss & 1);
				setFieldValue(word, inst, Field_d, d);
				setFieldValue(word, inst, Field_k, negate ? 1 : 0);
				setFieldValue(word, inst, Field_QQQQ, qqqq);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// MACSU/MACUU/MPYSU/MPYUU: [-]QQQQ,D
			case Macsu:
			case Mpy_su:
			{
				// Format: "macsu [-]x0,y0,a" or "mpyuu -x1,x0,b"
				// The mnemonic already includes the su/uu suffix
				bool negate = false;
				std::string operandsCopy = _operands;
				if (!operandsCopy.empty() && operandsCopy[0] == '-')
				{
					negate = true;
					operandsCopy = operandsCopy.substr(1);
				}

				std::vector<std::string> suOps;
				splitOperands(operandsCopy, suOps);
				if (suOps.size() != 3) continue;

				TWord qqqq;
				if (!parseRegister_QQQQ(suOps[0], suOps[1], qqqq)) continue;
				TWord d;
				if (!parseAluD(suOps[2], d)) continue;

				// Determine su/uu from the mnemonic
				bool uu = (_mnemonic.size() >= 5 && _mnemonic.substr(_mnemonic.size() - 2) == "uu");

				setFieldValue(word, inst, Field_s, uu ? 1 : 0);
				setFieldValue(word, inst, Field_d, d);
				setFieldValue(word, inst, Field_k, negate ? 1 : 0);
				setFieldValue(word, inst, Field_QQQQ, qqqq);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// CLB: S,D
			case Clb:
			{
				if (ops.size() != 2) continue;
				TWord s, d;
				if (!parseAluD(ops[0], s)) continue;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_S, s);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// CMPU: S1,S2 (ggg,d)
			case Cmpu_S1S2:
			{
				if (ops.size() != 2) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				TWord ggg;
				if (!parseRegister_ggg(ops[0], d != 0, ggg)) continue;
				setFieldValue(word, inst, Field_ggg, ggg);
				setFieldValue(word, inst, Field_d, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// NORMF: S,D (sss, D)
			case Normf:
			{
				if (ops.size() != 2) continue;
				TWord sss;
				if (!parseRegister_sss(ops[0], sss)) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_sss, sss);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// NORM: Rn,D
			case Norm:
			{
				if (ops.size() != 2) continue;
				TWord r;
				if (!parseRegisterR(ops[0], r)) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_RRR, r);
				setFieldValue(word, inst, Field_d, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// MERGE: S,D (SSS, D)
			case Merge:
			{
				if (ops.size() != 2) continue;
				TWord sss;
				if (!parseRegister_sss(ops[0], sss)) continue;
				TWord d;
				if (!parseAluD(ops[1], d)) continue;
				setFieldValue(word, inst, Field_SSS, sss);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit operations on D register: #n,D
			case Bchg_D: case Bclr_D: case Bset_D: case Btst_D:
			{
				if (ops.size() != 2) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;
				TWord d;
				if (!parseRegister_dddddd(ops[1], d)) continue;
				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_DDDDDD, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit operations on absolute address: #n,S:aa
			case Bchg_aa: case Bclr_aa: case Bset_aa: case Btst_aa:
			{
				if (ops.size() != 2) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;

				// Parse memory operand: x:$aa or y:$aa
				const auto mem = toLower(ops[1]);
				if (mem.size() < 3 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;
				if (area != MemArea_X && area != MemArea_Y) continue;

				auto addrStr = mem.substr(2);
				if (!addrStr.empty() && addrStr[0] == '<') addrStr = addrStr.substr(1);
				TWord addr;
				if (!parseNumber(addrStr, addr)) continue;
				if (addr > 0x3F) continue;

				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_aaaaaa, addr);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit operations on peripheral pp: #n,S:<<pp
			case Bchg_pp: case Bclr_pp: case Bset_pp: case Btst_pp:
			{
				if (ops.size() != 2) continue;

				const auto mem = toLower(ops[1]);
				if (mem.size() < 4 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;

				auto addrStr = mem.substr(2);
				if (addrStr.substr(0, 2) != "<<") continue;
				addrStr = addrStr.substr(2);
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(area, addrStr, addr)) continue;
				if (addr < 0xFFFFC0) continue;
				TWord pp = addr - 0xFFFFC0;
				if (pp > 0x3F) continue;

				// Parse bit number: either #$n or #SymbolName
				TWord b;
				if (!parseImmediate(ops[0], b))
				{
					auto bitName = ops[0];
					if (!bitName.empty() && bitName[0] == '#') bitName = bitName.substr(1);
					if (!resolveBitSymbol(area, addr, bitName, b)) continue;
				}
				if (b > 23) continue;

				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_pppppp, pp);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit operations on peripheral qq: #n,S:<<qq
			case Bchg_qq: case Bclr_qq: case Bset_qq: case Btst_qq:
			{
				if (ops.size() != 2) continue;

				const auto mem = toLower(ops[1]);
				if (mem.size() < 4 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;

				auto addrStr = mem.substr(2);
				if (addrStr.substr(0, 2) != "<<") continue;
				addrStr = addrStr.substr(2);
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(area, addrStr, addr)) continue;
				if (addr < 0xFFFF80 || addr >= 0xFFFFC0) continue;
				TWord qq = addr - 0xFFFF80;

				// Parse bit number: either #$n or #SymbolName
				TWord b;
				if (!parseImmediate(ops[0], b))
				{
					auto bitName = ops[0];
					if (!bitName.empty() && bitName[0] == '#') bitName = bitName.substr(1);
					if (!resolveBitSymbol(area, addr, bitName, b)) continue;
				}
				if (b > 23) continue;

				setFieldValue(word, inst, Field_qqqqqq, qq);
				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit operations on effective address: #n,S:ea
			case Bchg_ea: case Bclr_ea: case Bset_ea: case Btst_ea:
			{
				if (ops.size() != 2) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;

				const auto mem = toLower(ops[1]);
				if (mem.size() < 3 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;

				auto addrStr = mem.substr(2);
				AddressingMode am;
				if (!parseAddressingMode(addrStr, am)) continue;

				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// Bit-test-and-jump/branch on register: #n,S,addr
			case Jclr_S: case Jset_S: case Jsclr_S: case Jsset_S:
			case Brclr_S: case Brset_S: case Bsclr_S: case Bsset_S:
			{
				if (ops.size() != 3) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;
				TWord d;
				if (!parseRegister_dddddd(ops[1], d)) continue;
				TWord addr;
				auto addrStr = ops[2];
				if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
				if (!parseNumber(addrStr, addr)) continue;
				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_DDDDDD, d);
				result.word[0] = word;
				result.word[1] = addr;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit-test-and-jump/branch on aa: #n,S:aa,addr
			case Jclr_aa: case Jset_aa: case Jsclr_aa: case Jsset_aa:
			case Brclr_aa: case Brset_aa: case Bsclr_aa: case Bsset_aa:
			{
				if (ops.size() != 3) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;
				const auto mem = toLower(ops[1]);
				if (mem.size() < 3 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;
				auto aaStr = mem.substr(2);
				if (!aaStr.empty() && aaStr[0] == '<') aaStr = aaStr.substr(1);
				TWord aa;
				if (!parseNumber(aaStr, aa)) continue;
				if (aa > 0x3F) continue;
				TWord addr;
				auto addrStr = ops[2];
				if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
				if (!parseNumber(addrStr, addr)) continue;
				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_aaaaaa, aa);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.word[1] = addr;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit-test-and-jump/branch on pp: #n,S:<<pp,addr
			case Jclr_pp: case Jset_pp: case Jsclr_pp: case Jsset_pp:
			case Brclr_pp: case Brset_pp: case Bsclr_pp: case Bsset_pp:
			{
				if (ops.size() != 3) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;
				const auto mem = toLower(ops[1]);
				if (mem.size() < 4 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;
				auto addrStr = mem.substr(2);
				if (addrStr.substr(0, 2) != "<<") continue;
				addrStr = addrStr.substr(2);
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(area, addrStr, addr)) continue;
				if (addr < 0xFFFFC0) continue;
				TWord pp = addr - 0xFFFFC0;
				if (pp > 0x3F) continue;
				TWord target;
				auto targetStr = ops[2];
				if (!targetStr.empty() && targetStr[0] == '>') targetStr = targetStr.substr(1);
				if (!parseNumber(targetStr, target)) continue;
				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_pppppp, pp);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.word[1] = target;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Bit-test-and-jump/branch on qq: #n,S:<<qq,addr
			case Jclr_qq: case Jset_qq: case Jsclr_qq: case Jsset_qq:
			case Brclr_qq: case Brset_qq: case Bsclr_qq: case Bsset_qq:
			{
				if (ops.size() != 3) continue;
				TWord b;
				if (!parseImmediate(ops[0], b)) continue;
				if (b > 23) continue;
				const auto mem = toLower(ops[1]);
				if (mem.size() < 4 || mem[1] != ':') continue;
				EMemArea area;
				if (!parseMemoryArea(std::string(1, mem[0]), area)) continue;
				auto addrStr = mem.substr(2);
				if (addrStr.substr(0, 2) != "<<") continue;
				addrStr = addrStr.substr(2);
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(area, addrStr, addr)) continue;
				if (addr < 0xFFFF80 || addr >= 0xFFFFC0) continue;
				TWord qq = addr - 0xFFFF80;
				if (qq > 0x3F) continue;
				TWord target;
				auto targetStr = ops[2];
				if (!targetStr.empty() && targetStr[0] == '>') targetStr = targetStr.substr(1);
				if (!parseNumber(targetStr, target)) continue;
				setFieldValue(word, inst, Field_bbbbb, b);
				setFieldValue(word, inst, Field_qqqqqq, qq);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.word[1] = target;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Extract/Extractu with register: S1,S2,D (sss, s, D)
			case Extract_S1S2: case Extractu_S1S2:
			{
				if (ops.size() != 3) continue;
				TWord sss;
				if (!parseRegister_sss(ops[0], sss)) continue;
				TWord s, d;
				if (!parseAluD(ops[1], s)) continue;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_SSS, sss);
				setFieldValue(word, inst, Field_s, s);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Extract/Extractu with immediate: #CO,S2,D
			case Extract_CoS2: case Extractu_CoS2:
			{
				if (ops.size() != 3) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				TWord s, d;
				if (!parseAluD(ops[1], s)) continue;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_s, s);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.word[1] = imm;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Insert with register: S1,S2,D (sss, qqq, D)
			case Insert_S1S2:
			{
				if (ops.size() != 3) continue;
				TWord sss;
				if (!parseRegister_sss(ops[0], sss)) continue;
				TWord qqq;
				if (!parseRegister_qqq(ops[1], qqq)) continue;
				TWord d;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_SSS, sss);
				setFieldValue(word, inst, Field_qqq, qqq);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Insert with immediate: #CO,S2,D
			case Insert_CoS2:
			{
				if (ops.size() != 3) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				TWord qqq;
				if (!parseRegister_qqq(ops[1], qqq)) continue;
				TWord d;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_qqq, qqq);
				setFieldValue(word, inst, Field_D, d);
				result.word[0] = word;
				result.word[1] = imm;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Mpy_SD, Mac_S, Mpyr_SD, Macr_S: (+/-)QQ,#sssss,D
			case Mpy_SD: case Mac_S: case Mpyr_SD: case Macr_S:
			{
				if (ops.size() != 3) continue;
				bool negate = false;
				auto first = ops[0];
				if (!first.empty() && first[0] == '-') { negate = true; first = first.substr(1); }
				TWord qq;
				if (!parseRegister_QQ(first, qq)) continue;
				TWord imm;
				if (!parseImmediate(ops[1], imm)) continue;
				TWord d;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_QQ, qq);
				setFieldValue(word, inst, Field_sssss, imm);
				setFieldValue(word, inst, Field_d, d);
				setFieldValue(word, inst, Field_k, negate ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Mpyi, Maci_xxxx, Mpyri, Macri_xxxx: (+/-)#xxxx,qq,D
			case Mpyi: case Maci_xxxx: case Mpyri: case Macri_xxxx:
			{
				if (ops.size() != 3) continue;
				bool negate = false;
				auto first = ops[0];
				if (!first.empty() && first[0] == '-') { negate = true; first = first.substr(1); }
				TWord imm;
				if (!parseImmediate(first, imm)) continue;
				TWord qq;
				if (!parseRegister_qq(ops[1], qq)) continue;
				TWord d;
				if (!parseAluD(ops[2], d)) continue;
				setFieldValue(word, inst, Field_qq, qq);
				setFieldValue(word, inst, Field_d, d);
				setFieldValue(word, inst, Field_k, negate ? 1 : 0);
				result.word[0] = word;
				result.word[1] = imm;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Do with immediate: do #xxx,addr
			case Do_xxx:
			{
				if (ops.size() != 2) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0xFFF) continue;
				TWord addr;
				if (!parseNumber(ops[1].size() > 0 && ops[1][0] == '>' ? ops[1].substr(1) : ops[1], addr)) continue;
				setFieldValue(word, inst, Field_hhhh, imm >> 8);
				setFieldValue(word, inst, Field_iiiiiiii, imm & 0xFF);
				result.word[0] = word;
				result.word[1] = addr > 0 ? addr - 1 : 0; // loop end address is one less
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Do with register: do S,addr
			case Do_S:
			{
				if (ops.size() != 2) continue;
				TWord dd;
				if (!parseRegister_dddddd(ops[0], dd)) continue;
				TWord addr;
				std::string addrStr = ops[1];
				if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
				if (!parseNumber(addrStr, addr)) continue;
				setFieldValue(word, inst, Field_DDDDDD, dd);
				result.word[0] = word;
				result.word[1] = addr > 0 ? addr - 1 : 0;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// DoForever: do forever,addr
			case DoForever:
			{
				if (ops.size() != 1) continue;
				TWord addr;
				std::string addrStr = ops[0];
				if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
				if (!parseNumber(addrStr, addr)) continue;
				result.word[0] = word;
				result.word[1] = addr > 0 ? addr - 1 : 0;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// Rep with immediate: rep #xxx
			case Rep_xxx:
			{
				if (ops.size() != 1) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0xFFF) continue;
				setFieldValue(word, inst, Field_hhhh, imm >> 8);
				setFieldValue(word, inst, Field_iiiiiiii, imm & 0xFF);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Rep with register: rep S
			case Rep_S:
			{
				if (ops.size() != 1) continue;
				// Must be a register, not an immediate
				if (!ops[0].empty() && ops[0][0] == '#') continue;
				TWord dd;
				if (!parseRegister_dddddd(ops[0], dd)) continue;
				setFieldValue(word, inst, Field_dddddd, dd);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Movec_S1D2: move S,D (register to control register or vice versa)
			case Movec_S1D2:
			{
				if (ops.size() != 2) continue;
				// One operand must be a PCR register (DDDDD), the other a regular register (dddddd)
				TWord dd, ee;
				bool srcIsPCR = parseRegister_DDDDD(ops[0], dd);
				bool dstIsPCR = parseRegister_DDDDD(ops[1], dd);

				if (srcIsPCR && !dstIsPCR)
				{
					// PCR → regular register (write=0, S1 is PCR)
					parseRegister_DDDDD(ops[0], dd);
					if (!parseRegister_dddddd(ops[1], ee)) continue;
					setFieldValue(word, inst, Field_DDDDD, dd);
					setFieldValue(word, inst, Field_eeeeee, ee);
					setFieldValue(word, inst, Field_W, 0);
				}
				else if (dstIsPCR && !srcIsPCR)
				{
					// regular register → PCR (write=1)
					if (!parseRegister_dddddd(ops[0], ee)) continue;
					parseRegister_DDDDD(ops[1], dd);
					setFieldValue(word, inst, Field_DDDDD, dd);
					setFieldValue(word, inst, Field_eeeeee, ee);
					setFieldValue(word, inst, Field_W, 1);
				}
				else
				{
					continue; // ambiguous or both are PCR
				}
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Movec_ea: move x:(rN),D or move D,x:(rN) — EA to/from control register
			// Also handles immediate: move #>$aabbcc,lc (MMMRRR_ImmediateData)
			case Movec_ea:
			{
				if (ops.size() != 2) continue;
				const auto op0 = toLower(ops[0]);
				const auto op1 = toLower(ops[1]);

				// Determine which side is the PCR register and which is the EA
				TWord dd;
				bool op0IsPCR = parseRegister_DDDDD(op0, dd);
				bool op1IsPCR = parseRegister_DDDDD(op1, dd);

				if (!op0IsPCR && !op1IsPCR) continue;

				// Both PCR → ambiguous for this instruction
				if (op0IsPCR && op1IsPCR) continue;

				const auto& eaOp = op0IsPCR ? op1 : op0;
				const auto& pcrOp = op0IsPCR ? op0 : op1;
				bool write = !op0IsPCR; // W=1 means EA→PCR

				parseRegister_DDDDD(pcrOp, dd);

				// Check for long immediate: #>$xxxx (short immediates handled by Movec_xx)
				if (eaOp.size() > 2 && eaOp[0] == '#' && eaOp[1] == '>')
				{
					if (!write) continue; // immediate can only be source
					auto immStr = eaOp.substr(2);
					TWord imm;
					if (!parseNumber(immStr, imm)) continue;

					setFieldValue(word, inst, Field_DDDDD, dd);
					setFieldValue(word, inst, Field_MMM, MMMRRR_ImmediateData >> 3);
					setFieldValue(word, inst, Field_RRR, MMMRRR_ImmediateData & 7);
					setFieldValue(word, inst, Field_W, 1);
					setFieldValue(word, inst, Field_S, 0); // X memory space
					result.word[0] = word;
					result.word[1] = imm;
					result.wordCount = 2;
					result.error = AssembleError::OK;
					return result;
				}

				// Parse memory EA: x:(rN), y:(rN), etc.
				EMemArea area = MemArea_X;
				auto eaStr = eaOp;
				if (eaStr.size() > 2 && eaStr[1] == ':')
				{
					parseMemoryArea(std::string(1, eaStr[0]), area);
					eaStr = eaStr.substr(2);
				}

				bool isLongAddr = false;
				if (!eaStr.empty() && eaStr[0] == '>')
				{
					isLongAddr = true;
					eaStr = eaStr.substr(1);
				}

				AddressingMode am;
				if (!parseAddressingMode(eaStr, am))
				{
					// Try parsing as absolute address (only if forced long with > prefix)
					if (!isLongAddr) continue;
					TWord addr;
					if (!parseNumber(eaStr, addr)) continue;
					am.mmmrrr = MMMRRR_AbsAddr;
					am.extWord = addr;
					am.hasExtWord = true;
				}

				setFieldValue(word, inst, Field_DDDDD, dd);
				setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				setFieldValue(word, inst, Field_W, write ? 1 : 0);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// Movec_aa: move x:$aa,D or move D,x:$aa — absolute short address to/from control register
			case Movec_aa:
			{
				if (ops.size() != 2) continue;
				const auto op0 = toLower(ops[0]);
				const auto op1 = toLower(ops[1]);

				TWord dd;
				bool op0IsPCR = parseRegister_DDDDD(op0, dd);
				bool op1IsPCR = parseRegister_DDDDD(op1, dd);

				if (!op0IsPCR && !op1IsPCR) continue;
				if (op0IsPCR && op1IsPCR) continue;

				const auto& eaOp = op0IsPCR ? op1 : op0;
				const auto& pcrOp = op0IsPCR ? op0 : op1;
				bool write = !op0IsPCR; // W=1 means mem→PCR

				parseRegister_DDDDD(pcrOp, dd);

				// Parse memory operand: x:$aa or y:$aa
				EMemArea area = MemArea_X;
				auto addrStr = eaOp;
				if (addrStr.size() > 2 && addrStr[1] == ':')
				{
					parseMemoryArea(std::string(1, addrStr[0]), area);
					addrStr = addrStr.substr(2);
				}
				else
				{
					continue; // must have memory area prefix
				}

				TWord addr;
				if (!parseNumber(addrStr, addr)) continue;
				if (addr > 0x3F && addr < 0xFFFFC0) continue;
				addr &= 0x3F;

				setFieldValue(word, inst, Field_DDDDD, dd);
				setFieldValue(word, inst, Field_aaaaaa, addr);
				setFieldValue(word, inst, Field_W, write ? 1 : 0);
				setFieldValue(word, inst, Field_S, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Movec_xx: move #xx,D (immediate to control register)
			case Movec_xx:
			{
				if (ops.size() != 2) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0xFF) continue;
				TWord dd;
				if (!parseRegister_DDDDD(ops[1], dd)) continue;
				setFieldValue(word, inst, Field_iiiiiiii, imm);
				setFieldValue(word, inst, Field_DDDDD, dd);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Movem_ea: move S/D,p:ea
			case Movem_ea:
			{
				if (ops.size() != 2) continue;
				const auto op0 = toLower(ops[0]);
				const auto op1 = toLower(ops[1]);

				bool srcIsPmem = (op0.size() >= 2 && op0[0] == 'p' && op0[1] == ':');
				bool dstIsPmem = (op1.size() >= 2 && op1[0] == 'p' && op1[1] == ':');

				if (!srcIsPmem && !dstIsPmem) continue;

				const auto& memOp = srcIsPmem ? op0 : op1;
				const auto& regOp = srcIsPmem ? op1 : op0;
				bool write = srcIsPmem; // write = reading from p memory

				auto addrStr = memOp.substr(2);
				AddressingMode am;
				if (!parseAddressingMode(addrStr, am)) continue;

				TWord dd;
				if (!parseRegister_dddddd(regOp, dd)) continue;

				setFieldValue(word, inst, Field_dddddd, dd);
				setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				setFieldValue(word, inst, Field_W, write ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// Movex_Rnxxxx / Movey_Rnxxxx: move X/Y:(Rn+xxxx),D
			case Movex_Rnxxxx: case Movey_Rnxxxx:
			{
				if (ops.size() != 2) continue;
				const auto op0 = toLower(ops[0]);
				const auto op1 = toLower(ops[1]);
				const bool isY = (inst == Movey_Rnxxxx);
				const char expectedArea = isY ? 'y' : 'x';

				bool srcIsMem = (op0.size() >= 2 && op0[0] == expectedArea && op0[1] == ':');
				bool dstIsMem = (op1.size() >= 2 && op1[0] == expectedArea && op1[1] == ':');

				if (!srcIsMem && !dstIsMem) continue;

				const auto& memOp = srcIsMem ? op0 : op1;
				const auto& regOp = srcIsMem ? op1 : op0;
				bool write = srcIsMem;

				auto addrStr = memOp.substr(2);
				// Expect (rN+$xxxx) or (rN-$xxxx) format
				if (addrStr.empty() || addrStr[0] != '(') continue;
				auto closeParen = addrStr.find(')');
				if (closeParen == std::string::npos) continue;
				auto inner = addrStr.substr(1, closeParen - 1);

				// Parse rN+xxxx or rN-xxxx
				auto plusPos = inner.find('+');
				auto minusPos = inner.find('-');
				size_t opPos;
				bool isNeg = false;
				if (plusPos != std::string::npos) { opPos = plusPos; }
				else if (minusPos != std::string::npos) { opPos = minusPos; isNeg = true; }
				else continue;

				TWord r;
				if (!parseRegisterR(inner.substr(0, opPos), r)) continue;
				TWord offset;
				if (!parseNumber(inner.substr(opPos + 1), offset)) continue;
				if (isNeg) offset = static_cast<TWord>(-static_cast<int>(offset)) & 0xFFFFFF;

				TWord dd;
				if (!parseRegister_dddddd(regOp, dd)) continue;

				setFieldValue(word, inst, Field_RRR, r);
				setFieldValue(word, inst, Field_DDDDDD, dd);
				setFieldValue(word, inst, Field_W, write ? 1 : 0);
				result.word[0] = word;
				result.word[1] = offset;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// JMP: absolute address or ea
			case Jmp_xxx:
			{
				if (ops.size() != 1) continue;
				TWord addr;
				if (!parseNumber(ops[0], addr)) continue;
				if (addr > 0xFFF) continue;
				setFieldValue(word, inst, Field_aaaaaaaaaaaa, addr);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			case Jmp_ea:
			{
				if (ops.size() != 1) continue;
				AddressingMode am;
				if (!parseAddressingMode(ops[0], am)) continue;
				setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// JSR: absolute address or ea
			case Jsr_xxx:
			{
				if (ops.size() != 1) continue;
				TWord addr;
				if (!parseNumber(ops[0], addr)) continue;
				if (addr > 0xFFF) continue;
				setFieldValue(word, inst, Field_aaaaaaaaaaaa, addr);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			case Jsr_ea:
			{
				if (ops.size() != 1) continue;
				AddressingMode am;
				if (!parseAddressingMode(ops[0], am)) continue;
				setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// BRA: PC-relative branch with extension word
			case Bra_xxxx:
			{
				if (ops.size() != 1) continue;
				TWord addr;
				auto addrStr = ops[0];
				if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
				if (!parseNumber(addrStr, addr)) continue;
				result.word[0] = word;
				result.word[1] = addr;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// BSR: PC-relative branch to subroutine with extension word
			case Bsr_xxxx:
			{
				if (ops.size() != 1) continue;
				TWord addr;
				auto addrStr = ops[0];
				if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
				if (!parseNumber(addrStr, addr)) continue;
				result.word[0] = word;
				result.word[1] = addr;
				result.wordCount = 2;
				result.error = AssembleError::OK;
				return result;
			}

			// LUA/LEA ea,D
			case Lua_ea:
			{
				if (ops.size() != 2) continue;
				AddressingMode am;
				if (!parseAddressingMode(ops[0], am)) continue;
				TWord dd;
				const auto& regMap = getRegisterMap_DDDDDD();
				auto regIt = regMap.find(toLower(ops[1]));
				if (regIt == regMap.end()) continue;
				dd = regIt->second;
				setFieldValue(word, inst, Field_MM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				setFieldValue(word, inst, Field_ddddd, dd);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// LUA/LEA (Rn+aa),D
			case Lua_Rn:
			{
				if (ops.size() != 2) continue;
				auto addrStr = toLower(ops[0]);
				// Expect (rN+$aa) or (rN-$aa)
				if (addrStr.empty() || addrStr[0] != '(') continue;
				auto closeParen = addrStr.find(')');
				if (closeParen == std::string::npos) continue;
				auto inner = addrStr.substr(1, closeParen - 1);

				auto plusPos = inner.find('+');
				auto minusPos = inner.find('-');
				size_t opPos;
				bool isNeg = false;
				if (plusPos != std::string::npos) { opPos = plusPos; }
				else if (minusPos != std::string::npos) { opPos = minusPos; isNeg = true; }
				else continue;

				TWord r;
				if (!parseRegisterR(inner.substr(0, opPos), r)) continue;
				TWord offset;
				if (!parseNumber(inner.substr(opPos + 1), offset)) continue;
				int signedOffset = isNeg ? -static_cast<int>(offset) : static_cast<int>(offset);

				// 7-bit signed offset, encode as two fields: aaa (3 MSBs) and aaaa (4 LSBs)
				TWord aa = static_cast<TWord>(signedOffset) & 0x7F;

				// Parse destination: rN or nN (dddd encoding: 0-7 for r0-r7, 8-15 for n0-n7)
				const auto dstReg = toLower(ops[1]);
				TWord dddd;
				if (dstReg.size() == 2 && dstReg[0] == 'r' && dstReg[1] >= '0' && dstReg[1] <= '7')
					dddd = dstReg[1] - '0';
				else if (dstReg.size() == 2 && dstReg[0] == 'n' && dstReg[1] >= '0' && dstReg[1] <= '7')
					dddd = 8 + (dstReg[1] - '0');
				else continue;

				setFieldValue(word, inst, Field_RRR, r);
				setFieldValue(word, inst, Field_aaa, aa >> 4);
				setFieldValue(word, inst, Field_aaaa, aa & 0xF);
				setFieldValue(word, inst, Field_dddd, dddd);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Lra_xxxx: lra >*+$offset,D — load relative address
			case Lra_xxxx:
			{
				if (ops.size() != 2) continue;
				auto addrStr = toLower(ops[0]);
				// Format from disassembler: >*+$offset or >*-$offset or >*
				if (addrStr.empty() || addrStr[0] != '>') continue;
				addrStr = addrStr.substr(1);
				if (addrStr.empty() || addrStr[0] != '*') continue;
				addrStr = addrStr.substr(1);

				TWord offset = 0;
				if (!addrStr.empty())
				{
					bool isNeg = false;
					if (addrStr[0] == '+') addrStr = addrStr.substr(1);
					else if (addrStr[0] == '-') { isNeg = true; addrStr = addrStr.substr(1); }
					else continue;
					if (!parseNumber(addrStr, offset)) continue;
					if (isNeg) offset = static_cast<TWord>(-static_cast<int>(offset)) & 0xFFFFFF;
				}

				// Parse destination register: rN or nN (DDDDDD 5-bit encoding)
				const auto& regMap = getRegisterMap_DDDDDD();
				auto regIt = regMap.find(toLower(ops[1]));
				if (regIt == regMap.end()) continue;

				result.word[0] = word;
				result.word[1] = offset;
				result.wordCount = 2;
				setFieldValue(result.word[0], inst, Field_ddddd, regIt->second);
				result.error = AssembleError::OK;
				return result;
			}

			// Non-parallel Move_xx: #xx,D (8-bit immediate to register)
			case Move_xx:
			{
				if (ops.size() != 2) continue;
				TWord imm;
				if (!parseImmediate(ops[0], imm)) continue;
				if (imm > 0xFF) continue;
				TWord dd;
				const auto& regMap = getRegisterMap_DDDDDD();
				auto regIt = regMap.find(toLower(ops[1]));
				if (regIt == regMap.end()) continue;
				dd = regIt->second;
				setFieldValue(word, inst, Field_ddddd, dd);
				setFieldValue(word, inst, Field_iiiiiiii, imm);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Non-parallel Mover: S,D (register to register)
			case Mover:
			{
				if (ops.size() != 2) continue;
				const auto& regMap = getRegisterMap_DDDDDD();
				auto itSrc = regMap.find(toLower(ops[0]));
				auto itDst = regMap.find(toLower(ops[1]));
				if (itSrc == regMap.end() || itDst == regMap.end()) continue;
				setFieldValue(word, inst, Field_eeeee, itSrc->second);
				setFieldValue(word, inst, Field_ddddd, itDst->second);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// Non-parallel Movex/Movey with EA
			case Movex_ea: case Movey_ea:
			{
				if (ops.size() != 2) continue;
				const auto op0 = toLower(ops[0]);
				const auto op1 = toLower(ops[1]);
				const bool isY = (inst == Movey_ea);
				const char expectedArea = isY ? 'y' : 'x';

				bool srcIsMem = (op0.size() >= 2 && op0[0] == expectedArea && op0[1] == ':');
				bool dstIsMem = (op1.size() >= 2 && op1[0] == expectedArea && op1[1] == ':');

				if (!srcIsMem && !dstIsMem) continue;

				const auto& memOp = srcIsMem ? op0 : op1;
				const auto& regOp = srcIsMem ? op1 : op0;
				bool writeToReg = srcIsMem;

				auto addrStr = memOp.substr(2);
				AddressingMode am;
				if (!parseAddressingMode(addrStr, am)) continue;

				TWord dd;
				if (!parseRegister_dddddd(regOp, dd)) continue;
				if (dd > 0x1f) continue;

				setFieldValue(word, inst, Field_dd, dd >> 3);
				setFieldValue(word, inst, Field_ddd, dd & 0x7);
				setFieldValue(word, inst, Field_W, writeToReg ? 1 : 0);
				setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// Movex/Movey with absolute short address
			case Movex_aa: case Movey_aa:
			{
				if (ops.size() != 2) continue;
				const auto op0 = toLower(ops[0]);
				const auto op1 = toLower(ops[1]);
				const bool isY = (inst == Movey_aa);
				const char expectedArea = isY ? 'y' : 'x';

				bool srcIsMem = (op0.size() >= 2 && op0[0] == expectedArea && op0[1] == ':');
				bool dstIsMem = (op1.size() >= 2 && op1[0] == expectedArea && op1[1] == ':');

				if (!srcIsMem && !dstIsMem) continue;

				const auto& memOp = srcIsMem ? op0 : op1;
				const auto& regOp = srcIsMem ? op1 : op0;
				bool writeToReg = srcIsMem;

				auto addrStr = memOp.substr(2);
				if (!addrStr.empty() && addrStr[0] == '<') addrStr = addrStr.substr(1);
				TWord addr;
				if (!parseNumber(addrStr, addr)) continue;
				// Absolute short address check - but also allow negatively sign-extended values
				if (addr > 0x3F && addr < 0xFFFFC0) continue;
				addr &= 0x3F; // use 6-bit value

				TWord dd;
				if (!parseRegister_dddddd(regOp, dd)) continue;
				if (dd > 0x1f) continue;

				setFieldValue(word, inst, Field_dd, dd >> 3);
				setFieldValue(word, inst, Field_ddd, dd & 0x7);
				setFieldValue(word, inst, Field_W, writeToReg ? 1 : 0);
				setFieldValue(word, inst, Field_aaaaaa, addr);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// MOVEP: register to/from peripheral (SXqq and SYqq forms)
			case Movep_SXqq:
			case Movep_SYqq:
			{
				if (ops.size() != 2) continue;

				// Determine which operand is the peripheral address (has << prefix)
				int periphIdx = -1;
				for (int i = 0; i < 2; ++i)
				{
					auto op = toLower(ops[i]);
					if (op.size() > 3 && op[1] == ':' && op.substr(2, 2) == "<<")
						periphIdx = i;
				}
				if (periphIdx < 0) continue;

				auto periphStr = toLower(ops[periphIdx]);
				auto regStr = ops[1 - periphIdx];
				bool writeToReg = (periphIdx == 0); // peripheral is source → write to register

				// Parse peripheral address: x:<<$ffffXX or y:<<$ffffYY
				EMemArea area;
				if (!parseMemoryArea(std::string(1, periphStr[0]), area)) continue;
				if (inst == Movep_SXqq && area != MemArea_X) continue;
				if (inst == Movep_SYqq && area != MemArea_Y) continue;

				auto addrStr = periphStr.substr(4); // skip "x:<<"
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(area, addrStr, addr)) continue;
				TWord qq = addr - 0xFFFF80;
				if (qq > 0x3F) continue;

				// Parse register (dddddd)
				TWord dd;
				if (!parseRegister_dddddd(regStr, dd)) continue;

				setFieldValue(word, inst, Field_q, (qq >> 5) & 1);
				setFieldValue(word, inst, Field_qqqqq, qq & 0x1F);
				setFieldValue(word, inst, Field_dddddd, dd);
				setFieldValue(word, inst, Field_W, writeToReg ? 0 : 1);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// MOVEP: register to/from peripheral pp
			case Movep_Spp:
			{
				if (ops.size() != 2) continue;

				int periphIdx = -1;
				bool doubleChevron = false;
				for (int i = 0; i < 2; ++i)
				{
					auto op = toLower(ops[i]);
					if (op.size() > 3 && op[1] == ':')
					{
						if (op.substr(2, 2) == "<<")
						{
							periphIdx = i;
							doubleChevron = true;
						}
						else if (op[2] == '<')
						{
							periphIdx = i;
							doubleChevron = false;
						}
					}
				}
				if (periphIdx < 0) continue;

				auto periphStr = toLower(ops[periphIdx]);
				auto regStr = ops[1 - periphIdx];
				bool writeToReg = (periphIdx == 0);

				EMemArea area;
				if (!parseMemoryArea(std::string(1, periphStr[0]), area)) continue;

				auto addrStr = periphStr.substr(doubleChevron ? 4 : 3); // skip "x:<<" or "x:<"
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(area, addrStr, addr)) continue;
				TWord pp = addr - 0xFFFFC0;
				if (pp > 0x3F) continue;

				TWord dd;
				if (!parseRegister_dddddd(regStr, dd)) continue;

				setFieldValue(word, inst, Field_pppppp, pp);
				setFieldValue(word, inst, Field_dddddd, dd);
				setFieldValue(word, inst, Field_W, writeToReg ? 0 : 1);
				setFieldValue(word, inst, Field_s, area == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}

			// MOVEP: EA to/from peripheral qq with EA
			case Movep_Xqqea:
			case Movep_Yqqea:
			{
				if (ops.size() != 2) continue;

				int periphIdx = -1;
				for (int i = 0; i < 2; ++i)
				{
					auto op = toLower(ops[i]);
					if (op.size() > 3 && op[1] == ':' && op.substr(2, 2) == "<<")
						periphIdx = i;
				}
				if (periphIdx < 0) continue;

				auto periphStr = toLower(ops[periphIdx]);
				auto eaStr = ops[1 - periphIdx];
				bool writeToPeripheral = (periphIdx == 1);

				EMemArea periphArea;
				if (!parseMemoryArea(std::string(1, periphStr[0]), periphArea)) continue;
				if (inst == Movep_Xqqea && periphArea != MemArea_X) continue;
				if (inst == Movep_Yqqea && periphArea != MemArea_Y) continue;

				auto addrStr = periphStr.substr(4);
				TWord addr;
				if (!parseNumber(addrStr, addr) && !resolveSymbol(periphArea, addrStr, addr)) continue;
				TWord qq = addr - 0xFFFF80;
				if (qq > 0x3F) continue;

				// Parse EA side: can be x:(rN), y:(rN), p:>$addr, or #>$imm
				// This EA contains the memory area prefix
				EMemArea eaArea = MemArea_P;

				auto eaLower = toLower(eaStr);
				if (eaLower.size() > 2 && eaLower[1] == ':')
				{
					parseMemoryArea(std::string(1, eaLower[0]), eaArea);
					eaStr = eaStr.substr(2);
					if (!eaStr.empty() && eaStr[0] == '>') eaStr = eaStr.substr(1);
				}
				else if (eaLower.size() > 1 && eaLower[0] == '#')
				{
					// Immediate data: #>$xxxx
					auto immStr = eaStr.substr(1);
					if (!immStr.empty() && immStr[0] == '>') immStr = immStr.substr(1);
					TWord imm;
					if (!parseNumber(immStr, imm)) continue;

					setFieldValue(word, inst, Field_qqqqqq, qq);
					setFieldValue(word, inst, Field_MMM, MMMRRR_ImmediateData >> 3);
					setFieldValue(word, inst, Field_RRR, MMMRRR_ImmediateData & 7);
					setFieldValue(word, inst, Field_W, 1); // write to peripheral (immediate is always source)
					setFieldValue(word, inst, Field_S, eaArea == MemArea_Y ? 1 : 0);
					result.word[0] = word;
					result.word[1] = imm;
					result.wordCount = 2;
					result.error = AssembleError::OK;
					return result;
				}

				AddressingMode am;
				if (!parseAddressingMode(eaStr, am))
				{
					// Try parsing as absolute address
					TWord addr;
					if (!parseNumber(eaStr, addr)) continue;
					am.mmmrrr = MMMRRR_AbsAddr;
					am.extWord = addr;
					am.hasExtWord = true;
				}

				setFieldValue(word, inst, Field_qqqqqq, qq);
				setFieldValue(word, inst, Field_MMM, (am.mmmrrr >> 3) & 0x7);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 0x7);
				setFieldValue(word, inst, Field_W, writeToPeripheral ? 1 : 0);
				setFieldValue(word, inst, Field_S, eaArea == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			// MOVEP: EA to/from peripheral pp with EA
			case Movep_ppea:
			case Movep_eapp:
			{
				if (ops.size() != 2) continue;

				int periphIdx = -1;
				bool doubleChevron = false;
				for (int i = 0; i < 2; ++i)
				{
					auto op = toLower(ops[i]);
					if (op.size() > 2 && op[1] == ':')
					{
						if (op.substr(2, 2) == "<<")
						{
							periphIdx = i;
							doubleChevron = true;
						}
						else if (op[2] == '<')
						{
							periphIdx = i;
							doubleChevron = false;
						}
					}
				}
				if (periphIdx < 0) continue;

				auto periphStr = toLower(ops[periphIdx]);
				auto eaStr = ops[1 - periphIdx];
				bool writeToPeripheral = (periphIdx == 1);

				EMemArea periphArea;
				if (!parseMemoryArea(std::string(1, periphStr[0]), periphArea)) continue;

				auto ppAddrStr = periphStr.substr(doubleChevron ? 4 : 3);
				TWord ppAddr;
				if (!parseNumber(ppAddrStr, ppAddr) && !resolveSymbol(periphArea, ppAddrStr, ppAddr)) continue;
				TWord pp = ppAddr - 0xFFFFC0;
				if (pp > 0x3F) continue;

				// Parse EA
				auto eaLower = toLower(eaStr);
				EMemArea eaArea = MemArea_P;

				// Check for immediate: #>$xxxx
				if (eaLower.size() > 1 && eaLower[0] == '#')
				{
					auto immStr = eaStr.substr(1);
					if (!immStr.empty() && immStr[0] == '>') immStr = immStr.substr(1);
					TWord imm;
					if (!parseNumber(immStr, imm)) continue;

					setFieldValue(word, inst, Field_pppppp, pp);
					setFieldValue(word, inst, Field_MMM, MMMRRR_ImmediateData >> 3);
					setFieldValue(word, inst, Field_RRR, MMMRRR_ImmediateData & 7);
					setFieldValue(word, inst, Field_W, 1); // write to peripheral (immediate is always source)
					setFieldValue(word, inst, Field_s, periphArea == MemArea_Y ? 1 : 0);
					result.word[0] = word;
					result.word[1] = imm;
					result.wordCount = 2;
					result.error = AssembleError::OK;
					return result;
				}

				if (eaLower.size() > 2 && eaLower[1] == ':')
				{
					parseMemoryArea(std::string(1, eaLower[0]), eaArea);
					eaStr = eaStr.substr(2);
					if (!eaStr.empty() && eaStr[0] == '>') eaStr = eaStr.substr(1);
				}

				// Movep_ppea: EA must be in X or Y memory (not P)
				// Movep_eapp: EA must be in P memory
				if (inst == Movep_ppea && eaArea == MemArea_P) continue;
				if (inst == Movep_eapp && eaArea != MemArea_P) continue;

				AddressingMode am;
				if (!parseAddressingMode(eaStr, am))
				{
					// Try parsing as absolute address
					TWord addr;
					if (!parseNumber(eaStr, addr)) continue;
					am.mmmrrr = MMMRRR_AbsAddr;
					am.extWord = addr;
					am.hasExtWord = true;
				}

				setFieldValue(word, inst, Field_pppppp, pp);
				setFieldValue(word, inst, Field_MMM, (am.mmmrrr >> 3) & 0x7);
				setFieldValue(word, inst, Field_RRR, am.mmmrrr & 0x7);
				setFieldValue(word, inst, Field_W, writeToPeripheral ? 1 : 0);
				setFieldValue(word, inst, Field_s, periphArea == MemArea_Y ? 1 : 0);
				if (inst == Movep_ppea)
					setFieldValue(word, inst, Field_S, eaArea == MemArea_Y ? 1 : 0);
				result.word[0] = word;
				result.wordCount = 1;
				if (am.hasExtWord)
				{
					result.word[1] = am.extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}

			default:
				break;
			}
		}

		return result;
	}

	// ============= Parallel instruction encoding =============

	AssembleResult Assembler::assembleParallel(const std::string& _mnemonic, const std::string& _operands, const std::string& _moveStr) const
	{
		AssembleResult result;

		bool aluSuccess = false;
		TWord aluByte = encodeAlu(_mnemonic, _operands, aluSuccess);
		if (!aluSuccess)
			return result;

		// Check if moveStr is an IFCC (e.g., "ifeq", "ifge.u")
		if (_moveStr.size() >= 4 && _moveStr.substr(0, 2) == "if")
		{
			auto rest = _moveStr.substr(2);
			bool isU = false;
			if (rest.size() >= 2 && rest.substr(rest.size() - 2) == ".u")
			{
				isU = true;
				rest = rest.substr(0, rest.size() - 2);
			}
			TWord cccc;
			if (parseConditionCode(rest, cccc))
			{
				const auto inst = isU ? Ifcc_U : Ifcc;
				TWord moveWord = g_opcodes[inst].m_mask1 & 0xFFFF00;
				setFieldValue(moveWord, inst, Field_CCCC, cccc);
				result.word[0] = moveWord | aluByte;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}
		}

		TWord extWord = 0;
		bool hasExt = false;
		bool moveSuccess = false;
		TWord moveWord = encodeParallelMove(_moveStr, extWord, hasExt, moveSuccess);
		if (!moveSuccess)
			return result;

		result.word[0] = moveWord | aluByte;
		result.wordCount = 1;
		if (hasExt)
		{
			result.word[1] = extWord;
			result.wordCount = 2;
		}
		result.error = AssembleError::OK;
		return result;
	}

	// ============= Main assemble function =============

	AssembleResult Assembler::assemble(const char* _text) const
	{
		AssembleResult result;

		if (!_text || !*_text)
		{
			result.error = AssembleError::InvalidInstruction;
			return result;
		}

		auto text = toLower(std::string(_text));

		// Trim leading/trailing whitespace
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();

		if (text.empty())
		{
			result.error = AssembleError::InvalidInstruction;
			return result;
		}

		// Handle special case: nop → 0x000000
		if (text == "nop")
		{
			result.word[0] = 0;
			result.wordCount = 1;
			result.error = AssembleError::OK;
			return result;
		}

		// Handle ifcc / ifcc.u
		if (text.substr(0, 2) == "if")
		{
			auto rest = text.substr(2);
			bool isU = false;
			if (rest.size() >= 2 && rest.substr(rest.size() - 2) == ".u")
			{
				isU = true;
				rest = rest.substr(0, rest.size() - 2);
			}
			TWord cccc;
			if (parseConditionCode(rest, cccc))
			{
				const auto inst = isU ? Ifcc_U : Ifcc;
				TWord word = g_opcodes[inst].m_mask1;
				setFieldValue(word, inst, Field_CCCC, cccc);
				result.word[0] = word;
				result.wordCount = 1;
				result.error = AssembleError::OK;
				return result;
			}
		}

		// Tokenize: first token is mnemonic, rest are operands/moves
		auto tokens = tokenize(text);
		if (tokens.empty())
		{
			result.error = AssembleError::InvalidInstruction;
			return result;
		}

		auto mnemonic = tokens[0];
		std::string operands;
		std::string moveStr;

		// Check for condition code suffix on mnemonic
		// Mnemonics like bcc, jne, tge, etc.
		std::string condSuffix;
		std::string baseMnem = mnemonic;

		// Try to find the mnemonic as-is first
		auto it = m_mnemonics.find(baseMnem);

		if (it == m_mnemonics.end())
		{
			// Try extracting condition code suffix
			// Possible prefixes: b, bs, j, js, brk, debug, t, trap
			static const std::string ccPrefixes[] = { "trap", "debug", "brk", "bs", "b", "js", "j", "t" };
			for (const auto& prefix : ccPrefixes)
			{
				if (mnemonic.size() > prefix.size() && mnemonic.substr(0, prefix.size()) == prefix)
				{
					auto cc = mnemonic.substr(prefix.size());
					TWord ccVal;
					if (parseConditionCode(cc, ccVal))
					{
						baseMnem = prefix;
						condSuffix = cc;
						it = m_mnemonics.find(baseMnem);
						break;
					}
				}
			}
		}

		if (it == m_mnemonics.end())
		{
			// Try "do forever," as a special mnemonic
			if (mnemonic == "do" && tokens.size() >= 2 && toLower(tokens[1]).substr(0, 8) == "forever,")
			{
				auto addrStr = toLower(tokens[1]).substr(8);
				if (addrStr.empty() && tokens.size() >= 3)
					addrStr = tokens[2];
				// Handle "do forever, addr"
				it = m_mnemonics.find("do forever,");
				if (it != m_mnemonics.end())
				{
					TWord addr;
					if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
					if (parseNumber(addrStr, addr))
					{
						const auto inst = DoForever;
						TWord word = g_opcodes[inst].m_mask1;
						result.word[0] = word;
						result.word[1] = addr > 0 ? addr - 1 : 0;
						result.wordCount = 2;
						result.error = AssembleError::OK;
						return result;
					}
				}
			}

			// Still not found
			if (it == m_mnemonics.end())
			{
				result.error = AssembleError::InvalidInstruction;
				return result;
			}
		}

		// Build operand string from remaining tokens
		if (tokens.size() > 1)
			operands = tokens[1];

		// For parallel instructions, additional tokens may be move operations
		if (tokens.size() > 2)
		{
			// Token 2+ could be parallel move
			for (size_t i = 2; i < tokens.size(); ++i)
			{
				if (!moveStr.empty()) moveStr += " ";
				moveStr += tokens[i];
			}
		}

		// Special case: dmac has sign mode as separate token: "dmac ss x0,x0,a"
		// The sign mode "ss"/"su"/"us"/"uu" is in operands, actual QQQQ,D in moveStr
		if (baseMnem == "dmac" && !moveStr.empty())
		{
			operands = operands + " " + moveStr;
			moveStr.clear();
		}

		// Handle condition code instructions
		if (!condSuffix.empty())
		{
			TWord cccc;
			parseConditionCode(condSuffix, cccc);

			for (const auto inst : it->second)
			{
				const auto& oi = g_opcodes[inst];
				if (!hasField(oi, Field_CCCC)) continue;
				if (!isNonParallelOpcode(oi)) continue;

				TWord word = oi.m_mask1;
				setFieldValue(word, inst, Field_CCCC, cccc);

				switch (inst)
				{
				// Bcc with extension word (xxxx)
				case Bcc_xxxx: case BScc_xxxx:
				{
					if (operands.empty()) continue;
					auto addrStr = operands;
					if (!addrStr.empty() && addrStr[0] == '>') addrStr = addrStr.substr(1);
					TWord addr;
					if (!parseNumber(addrStr, addr)) continue;
					// PC-relative address stored in extension word
					result.word[0] = word;
					result.word[1] = addr;
					result.wordCount = 2;
					result.error = AssembleError::OK;
					return result;
				}

				// Bcc with register
				case Bcc_Rn: case BScc_Rn:
				{
					TWord r;
					if (!parseRegisterR(operands, r)) continue;
					setFieldValue(word, inst, Field_RRR, r);
					result.word[0] = word;
					result.wordCount = 1;
					result.error = AssembleError::OK;
					return result;
				}

				// Jcc with short address
				case Jcc_xxx: case Jscc_xxx:
				{
					TWord addr;
					if (!parseNumber(operands, addr)) continue;
					if (addr > 0xFFF) continue;
					setFieldValue(word, inst, Field_aaaaaaaaaaaa, addr);
					result.word[0] = word;
					result.wordCount = 1;
					result.error = AssembleError::OK;
					return result;
				}

				// Jcc with EA
				case Jcc_ea: case Jscc_ea:
				{
					AddressingMode am;
					if (!parseAddressingMode(operands, am)) continue;
					setFieldValue(word, inst, Field_MMM, am.mmmrrr >> 3);
					setFieldValue(word, inst, Field_RRR, am.mmmrrr & 7);
					result.word[0] = word;
					result.wordCount = 1;
					if (am.hasExtWord)
					{
						result.word[1] = am.extWord;
						result.wordCount = 2;
					}
					result.error = AssembleError::OK;
					return result;
				}

				// Tcc: S1,D1 [S2,D2]
				case Tcc_S1D1:
				{
					std::vector<std::string> ops;
					splitOperands(operands, ops);
					if (ops.size() != 2) continue;
					TWord d;
					if (!parseAluD(ops[1], d)) continue;
					TWord jjj;
					if (!parseRegister_JJJ(ops[0], d != 0, jjj)) continue;
					// JJJ < 4 overlaps with parallel move encoding, restrict to safe values
					if (jjj < 4) continue;
					setFieldValue(word, inst, Field_JJJ, jjj);
					setFieldValue(word, inst, Field_d, d);
					result.word[0] = word;
					result.wordCount = 1;
					result.error = AssembleError::OK;
					return result;
				}

				case Tcc_S1D1S2D2:
				{
					// Format: S1,D1 S2,D2 (operands has S1,D1, moveStr has S2,D2)
					std::vector<std::string> ops1, ops2;
					splitOperands(operands, ops1);
					if (ops1.size() != 2) continue;
					if (moveStr.empty()) continue;
					splitOperands(moveStr, ops2);
					if (ops2.size() != 2) continue;

					TWord d;
					if (!parseAluD(ops1[1], d)) continue;
					TWord jjj;
					if (!parseRegister_JJJ(ops1[0], d != 0, jjj)) continue;
					if (jjj < 4) continue;
					TWord ttt, TTT;
					if (!parseRegisterR(ops2[0], ttt)) continue;
					if (!parseRegisterR(ops2[1], TTT)) continue;
					setFieldValue(word, inst, Field_JJJ, jjj);
					setFieldValue(word, inst, Field_d, d);
					setFieldValue(word, inst, Field_ttt, ttt);
					setFieldValue(word, inst, Field_TTT, TTT);
					result.word[0] = word;
					result.wordCount = 1;
					result.error = AssembleError::OK;
					return result;
				}

				case Tcc_S2D2:
				{
					std::vector<std::string> ops;
					splitOperands(operands, ops);
					if (ops.size() != 2) continue;
					TWord ttt, TTT;
					if (!parseRegisterR(ops[0], ttt)) continue;
					if (!parseRegisterR(ops[1], TTT)) continue;
					setFieldValue(word, inst, Field_ttt, ttt);
					setFieldValue(word, inst, Field_TTT, TTT);
					result.word[0] = word;
					result.wordCount = 1;
					result.error = AssembleError::OK;
					return result;
				}

				case BRKcc: case Debugcc: case Trapcc:
				{
					result.word[0] = word;
					result.wordCount = 1;
					result.error = AssembleError::OK;
					return result;
				}

				default:
					break;
				}
			}

			result.error = AssembleError::InvalidInstruction;
			return result;
		}

		// Try parallel encoding first for ALU mnemonics that have parallel forms
		result = assembleParallel(baseMnem, operands, moveStr);
		if (result.success())
			return result;

		// Try non-parallel encoding
		result = assembleNonParallel(baseMnem, operands);
		if (result.success())
			return result;

		// If mnemonic is "move", try as a parallel move with no ALU
		if (baseMnem == "move")
		{
			// Try Movexy: "move xSrc,x:ea ySrc,y:ea" where operands=X move, moveStr=Y move
			if (!moveStr.empty())
			{
				auto xyResult = encodeMovexy(operands, moveStr);
				if (xyResult.success())
					return xyResult;
			}

			TWord extWord = 0;
			bool hasExt = false;
			bool moveSuccess = false;
			TWord moveWord = encodeParallelMove(operands, extWord, hasExt, moveSuccess);
			if (moveSuccess)
			{
				result.word[0] = moveWord;
				result.wordCount = 1;
				if (hasExt)
				{
					result.word[1] = extWord;
					result.wordCount = 2;
				}
				result.error = AssembleError::OK;
				return result;
			}
		}

		result.error = AssembleError::InvalidInstruction;
		return result;
	}
}
