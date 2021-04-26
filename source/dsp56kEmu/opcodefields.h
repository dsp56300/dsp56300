#pragma once
#include <cstdint>

namespace dsp56k
{
	struct FieldInfo
	{
		uint32_t bit;
		uint32_t len;
		uint32_t mask;

		constexpr explicit FieldInfo(const uint32_t _pos = 0, const uint32_t _len = 0) : bit(_pos), len(_len), mask((1<<_len)-1) {}
	};

	struct FieldParseConfig
	{
		const char ch;
		const uint32_t count;

		constexpr explicit FieldParseConfig(const char _ch, const uint32_t _count) : ch(_ch), count(_count) {}
	};

	constexpr static FieldInfo initField(const char* _opcode, const char _c, const uint32_t _count)
	{
		constexpr auto len = 24;

		for(uint32_t i=0; i<len;)
		{
			if(_opcode[i] != _c)
			{
				++i;
				continue;					
			}

			uint32_t count = 1;
			for(auto j=i+1; j<len; ++j)
			{
				if(_opcode[j] != _c)
					break;
				++count;
			}
			if(count == _count)
			{
				const auto bit = 23 - i;
				return FieldInfo(bit + 1 - count, count);
			}

			i += count;
		}
		return FieldInfo(0,0);
	}

	constexpr FieldParseConfig g_fieldParseConfigs[] =
	{
		FieldParseConfig('a', 1),		// Field_a
		FieldParseConfig('a', 3),		// Field_aaa
		FieldParseConfig('a', 4),		// Field_aaaa
		FieldParseConfig('a', 5),		// Field_aaaaa
		FieldParseConfig('a', 6),		// Field_aaaaaa
		FieldParseConfig('a', 12),		// Field_aaaaaaaaaaaa
		FieldParseConfig('b', 5),		// Field_bbbbb
		FieldParseConfig('C', 4),		// Field_CCCC
		FieldParseConfig('d', 1),		// Field_d
		FieldParseConfig('d', 2),		// Field_dd
		FieldParseConfig('d', 3),		// Field_ddd,			
		FieldParseConfig('d', 4),		// Field_dddd,			
		FieldParseConfig('d', 5),		// Field_ddddd,		
		FieldParseConfig('d', 6),		// Field_dddddd,		
		FieldParseConfig('D', 1),		// Field_D,			
		FieldParseConfig('D', 4),		// Field_DDDD,			
		FieldParseConfig('D', 5),		// Field_DDDDD,		
		FieldParseConfig('D', 6),		// Field_DDDDDD,		
		FieldParseConfig('e', 1),		// Field_e,			
		FieldParseConfig('e', 2),		// Field_ee,			
		FieldParseConfig('E', 2),		// Field_EE,			
		FieldParseConfig('e', 5),		// Field_eeeee,		
		FieldParseConfig('e', 6),		// Field_eeeeee,
		FieldParseConfig('f', 2),		// Field_ff,			
		FieldParseConfig('F', 1),		// Field_F,			
		FieldParseConfig('g', 3),		// Field_ggg,			
		FieldParseConfig('h', 4),		// Field_hhhh,			
		FieldParseConfig('i', 1),		// Field_i,			
		FieldParseConfig('i', 5),		// Field_iiiii,		
		FieldParseConfig('i', 6),		// Field_iiiiii,		
		FieldParseConfig('i', 8),		// Field_iiiiiiii,		
		FieldParseConfig('J', 1),		// Field_J,			
		FieldParseConfig('J', 2),		// Field_JJ,			
		FieldParseConfig('J', 3),		// Field_JJJ,			
		FieldParseConfig('k', 1),		// Field_k,			
		FieldParseConfig('L', 1),		// Field_L,			
		FieldParseConfig('L', 2),		// Field_LL,			
		FieldParseConfig('m', 2),		// Field_mm,			
		FieldParseConfig('M', 2),		// Field_MM,			
		FieldParseConfig('M', 3),		// Field_MMM,			
		FieldParseConfig('p', 6),		// Field_pppppp,		
		FieldParseConfig('q', 2),		// Field_qq,			
		FieldParseConfig('q', 3),		// Field_qqq,			
		FieldParseConfig('q', 1),		// Field_q,			
		FieldParseConfig('q', 5),		// Field_qqqqq,		
		FieldParseConfig('q', 6),		// Field_qqqqqq,		
		FieldParseConfig('Q', 2),		// Field_QQ,			
		FieldParseConfig('Q', 3),		// Field_QQQ,			
		FieldParseConfig('Q', 4),		// Field_QQQQ,			
		FieldParseConfig('r', 2),		// Field_rr,			
		FieldParseConfig('R', 3),		// Field_RRR,			
		FieldParseConfig('s', 1),		// Field_s,			
		FieldParseConfig('s', 3),		// Field_sss,			
		FieldParseConfig('s', 4),		// Field_ssss,			
		FieldParseConfig('s', 5),		// Field_sssss,		
		FieldParseConfig('S', 1),		// Field_S,			
		FieldParseConfig('S', 3),		// Field_SSS,			
		FieldParseConfig('t', 3),		// Field_ttt,			
		FieldParseConfig('T', 3),		// Field_TTT,			
		FieldParseConfig('w', 1),		// Field_w,			
		FieldParseConfig('W', 1),		// Field_W,			
		FieldParseConfig('l', 1),		// Field_l,		
		FieldParseConfig('o', 1),		// Field_o,			
		FieldParseConfig('o', 2),		// Field_oo,		
		FieldParseConfig('o', 3),		// Field_ooo,			
		FieldParseConfig('o', 4),		// Field_oooo,			
		FieldParseConfig('o', 5),		// Field_ooooo,			
		FieldParseConfig('o', 6),		// Field_oooooo,	
		FieldParseConfig('?', 8),		// Field_AluOperation		
		FieldParseConfig('?', 16)		// Field_MoveOperation
	};
}
