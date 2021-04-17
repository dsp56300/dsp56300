#pragma once

#include "dsp.h"
#include "types.h"
#include "utils.h"

#include "dsp_ops_helper.inl"

namespace dsp56k
{
	template <> inline void DSP::jumpOrJSR<false>(const TWord _ea)	{ setPC(_ea); }
	template <> inline void DSP::jumpOrJSR<true>(const TWord _ea)	{ jsr(_ea); }

	template<Instruction Inst, bool Jsr> void DSP::jumpIfCC(const TWord op, const TWord ea)
	{
		if(checkCondition<Inst>(op))
			jumpOrJSR<Jsr>(ea);
	}

	template<Instruction Inst, bool Jsr> void DSP::jumpIfCC(const TWord op)
	{
		const auto ea = effectiveAddress<Inst>(op);
		jumpIfCC<Inst,Jsr>(op, ea);
	}

	// ____________________________________________
	//

	inline void DSP::op_Jcc_xxx(const TWord op)	{ jumpIfCC<Jcc_xxx, false>(op); }
	inline void DSP::op_Jcc_ea(const TWord op)	{ jumpIfCC<Jcc_ea, false>(op); }

	inline void DSP::op_Jclr_ea(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(!bitTestMemory<Jclr_ea>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jclr_aa(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(!bitTestMemory<Jclr_aa>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jclr_pp(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(!bitTestMemory<Jclr_pp>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jclr_qq(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(!bitTestMemory<Jclr_qq>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jclr_S(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if( !bitTest<Jclr_S>( op, regValueDDDDDD<Jclr_S>(op) ) )
			setPC(addr);
	}
	// jmp
	inline void DSP::op_Jmp_ea(const TWord op)
	{
		setPC(effectiveAddress<Jmp_ea>(op));
	}
	inline void DSP::op_Jmp_xxx(const TWord op)
	{
		setPC(effectiveAddress<Jmp_xxx>(op));
	}
	// jscc
	inline void DSP::op_Jscc_xxx(const TWord op)
	{
		jumpIfCC<Jscc_xxx, true>(op);
	}
	inline void DSP::op_Jscc_ea(const TWord op)
	{
		jumpIfCC<Jscc_ea, true>(op);
	}
	inline void DSP::op_Jsclr_ea(const TWord op)
	{
		const auto addr = absAddressExt<Jsclr_ea>();
		if(!bitTestMemory<Jsclr_ea>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsclr_aa(const TWord op)
	{
		const auto addr = absAddressExt<Jsclr_aa>();
		if(!bitTestMemory<Jsclr_aa>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsclr_pp(const TWord op)
	{
		const auto addr = absAddressExt<Jsclr_pp>();
		if(!bitTestMemory<Jsclr_pp>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsclr_qq(const TWord op)
	{
		const auto addr = absAddressExt<Jsclr_qq>();
		if(!bitTestMemory<Jsclr_qq>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsclr_S(const TWord op)
	{
		const auto addr = fetchOpWordB();
		if(!bitTest<Jsclr_S>(op, regValueDDDDDD<Jsclr_S>(op)))
			jsr(addr);
	}
	inline void DSP::op_Jset_ea(const TWord op)
	{
		const TWord bit		= getFieldValue<Jset_ea,Field_bbbbb>(op);
		const TWord mmmrrr	= getFieldValue<Jset_ea,Field_MMM, Field_RRR>(op);
		const EMemArea S	= getFieldValueMemArea<Jset_ea>(op);

		const TWord val		= memRead( S, decode_MMMRRR_read(mmmrrr) );

		if( bittest(val,bit) )
		{
			setPC(val);
		}
	}
	inline void DSP::op_Jset_aa(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(bitTestMemory<Jclr_ea>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jset_pp(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(bitTestMemory<Jclr_pp>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jset_qq(const TWord op)
	{
		const auto addr = fetchOpWordB();

		if(bitTestMemory<Jclr_qq>(op))
		{
			setPC(addr);
		}
	}
	inline void DSP::op_Jset_S(const TWord op)
	{
		const TWord bit		= getFieldValue<Jset_S,Field_bbbbb>(op);
		const TWord dddddd	= getFieldValue<Jset_S,Field_DDDDDD>(op);

		const TWord addr	= fetchOpWordB();

		const TReg24 var	= decode_dddddd_read( dddddd );

		if( bittest(var,bit) )
		{
			setPC(addr);
		}		
	}
	inline void DSP::op_Jsr_ea(const TWord op)			{ jsr(effectiveAddress<Jsr_ea>(op)); }
	inline void DSP::op_Jsr_xxx(const TWord op)			{ jsr(effectiveAddress<Jsr_xxx>(op)); }
	inline void DSP::op_Jsset_ea(const TWord op)
	{
		const auto addr = fetchOpWordB();
		if(bitTestMemory<Jsset_ea>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsset_aa(const TWord op)
	{
		const auto addr = fetchOpWordB();
		if(bitTestMemory<Jsset_aa>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsset_pp(const TWord op)
	{
		const auto addr = fetchOpWordB();
		if(bitTestMemory<Jsset_pp>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsset_qq(const TWord op)
	{
		const auto addr = fetchOpWordB();
		if(bitTestMemory<Jsset_qq>(op))
			jsr(addr);
	}
	inline void DSP::op_Jsset_S(const TWord op)
	{
		const TWord dddddd = getFieldValue<Jsset_S,Field_DDDDDD>(op);
		const TWord bit = getFieldValue<Jsset_S,Field_bbbbb>(op);
		const int ea = fetchOpWordB();

		const TReg24 r = decode_dddddd_read( dddddd );

		if( bittest( r.var, bit ) )
		{
			jsr(TReg24(ea));
		}
	}
}
