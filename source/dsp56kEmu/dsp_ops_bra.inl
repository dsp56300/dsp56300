#pragma once
#include "dsp.h"
#include "utils.h"

#include "opcodetypes.h"

namespace dsp56k
{
	// ________________________________
	//

	// Bcc
	inline void DSP::op_Bcc_xxxx(const TWord op)
	{
		const TWord cccc	= getFieldValue<Bcc_xxxx,Field_CCCC>(op);
		const auto displacement = signextend<int, 24>(fetchOpWordB());

		if( decode_cccc( cccc ) )
		{
			setPC(pcCurrentInstruction + displacement);
		}
	}
	inline void DSP::op_Bcc_xxx(const TWord op)
	{
		const TWord cccc	= getFieldValue<Bcc_xxx,Field_CCCC>(op);

		if( decode_cccc( cccc ) )
		{
			const TWord a		= getFieldValue<Bcc_xxx,Field_aaaa, Field_aaaaa>(op);

			const TWord disp	= signextend<int,9>( a );
			assert( disp >= 0 );

			setPC(pcCurrentInstruction + disp);
		}
	}

	// Bra
	inline void DSP::op_Bra_xxxx(const TWord op)
	{
		const int displacement = signextend<int,24>(fetchOpWordB());
		setPC(pcCurrentInstruction + displacement);
	}

	inline void DSP::op_Bra_xxx(const TWord op)
	{
		const TWord addr = getFieldValue<Bra_xxx,Field_aaaa, Field_aaaaa>(op);

		const int displacement = signextend<int,9>(addr);

		setPC(pcCurrentInstruction + displacement);
	}
	inline void DSP::op_Bra_Rn(const TWord op)
	{
		errNotImplemented("BRA_Rn");
	}

	// Brclr
	inline void DSP::op_Brclr_ea(const TWord op)
	{
		errNotImplemented("BRCLR");			
	}
	inline void DSP::op_Brclr_aa(const TWord op)
	{
		errNotImplemented("BRCLR");			
	}
	inline void DSP::op_Brclr_pp(const TWord op)
	{
		const TWord bit		= getFieldValue<Brclr_pp,Field_bbbbb>(op);
		const TWord pppppp	= getFieldValue<Brclr_pp,Field_pppppp>(op);
		const EMemArea S	= getFieldValueMemArea<Brclr_pp>(op);

		const TWord ea = pppppp;

		const int displacement = signextend<int,24>(fetchOpWordB());

		if( !bittest( memReadPeriphFFFFC0( S, ea ), bit ) )
		{
			setPC(pcCurrentInstruction + displacement);
		}
	}
	inline void DSP::op_Brclr_qq(const TWord op)
	{
		const TWord bit		= getFieldValue<Brclr_qq,Field_bbbbb>(op);
		const TWord qqqqqq	= getFieldValue<Brclr_qq,Field_qqqqqq>(op);
		const EMemArea S	= getFieldValueMemArea<Brclr_qq>(op);

		const TWord ea = qqqqqq;

		const int displacement = signextend<int,24>(fetchOpWordB());

		if( !bittest( memReadPeriphFFFF80( S, ea ), bit ) )
		{
			setPC(pcCurrentInstruction + displacement);
		}
	}
	inline void DSP::op_Brclr_S(const TWord op)
	{
		const TWord bit		= getFieldValue<Brclr_S,Field_bbbbb>(op);
		const TWord dddddd	= getFieldValue<Brclr_S,Field_DDDDDD>(op);

		const TReg24 tst = decode_dddddd_read( dddddd );

		const int displacement = signextend<int,24>(fetchOpWordB());

		if( !bittest( tst, bit ) )
		{
			setPC(pcCurrentInstruction + displacement);
		}
	}
	// Brset
	inline void DSP::op_Brset_ea(const TWord op)	// BRSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 1 b b b b b
	{
		errNotImplemented("BRSET");
	}
	inline void DSP::op_Brset_aa(const TWord op)	// BRSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 1 b b b b b
	{
		errNotImplemented("BRSET");
	}
	inline void DSP::op_Brset_pp(const TWord op)	// BRSET #n,[X or Y]:pp,xxxx - 
	{
		errNotImplemented("BRSET");
	}
	inline void DSP::op_Brset_qq(const TWord op)
	{
		const TWord bit		= getFieldValue<Brset_qq,Field_bbbbb>(op);
		const TWord qqqqqq	= getFieldValue<Brset_qq,Field_qqqqqq>(op);
		const EMemArea S	= getFieldValueMemArea<Brset_qq>(op);

		const TWord ea = qqqqqq;

		const int displacement = signextend<int,24>( fetchOpWordB() );

		if( bittest( memReadPeriphFFFF80( S, ea ), bit ) )
		{
			setPC(pcCurrentInstruction + displacement);
		}
	}
	inline void DSP::op_Brset_S(const TWord op)
	{
		const TWord bit		= getFieldValue<Brset_S,Field_bbbbb>(op);
		const TWord dddddd	= getFieldValue<Brset_S,Field_DDDDDD>(op);

		const TReg24 r = decode_dddddd_read( dddddd );

		const int displacement = signextend<int,24>( fetchOpWordB() );

		if( bittest( r.var, bit ) )
		{
			setPC(pcCurrentInstruction + displacement);
		}
	}

	// BScc
	inline void DSP::op_BScc_xxxx(const TWord op)
	{
		const TWord cccc = getFieldValue<BScc_xxxx,Field_CCCC>(op);

		const int displacement = signextend<int,24>(fetchOpWordB());

		if( decode_cccc(cccc) )
		{
			jsr(pcCurrentInstruction + displacement);
		}
	}
	inline void DSP::op_BScc_xxx(const TWord op)
	{
		const TWord cccc = getFieldValue<BScc_xxx,Field_CCCC>(op);

		if( decode_cccc(cccc) )
		{
			const TWord addr = getFieldValue<BScc_xxx,Field_aaaa, Field_aaaaa>(op);

			const int displacement = signextend<int,9>(addr);

			jsr(pcCurrentInstruction + displacement);
		}
	}

	inline void DSP::op_BScc_Rn(const TWord op)
	{
		errNotImplemented("BScc Rn");
	}

	// Bsclr
	inline void DSP::op_Bsclr_ea(const TWord op)
	{
		errNotImplemented("BSCLR");
	}
	inline void DSP::op_Bsclr_aa(const TWord op)
	{
		errNotImplemented("BSCLR");
	}
	inline void DSP::op_Bsclr_pp(const TWord op)
	{
		errNotImplemented("BSCLR");
	}
	inline void DSP::op_Bsclr_qq(const TWord op)
	{
		errNotImplemented("BSCLR");		
	}
	inline void DSP::op_Bsclr_S(const TWord op)
	{
		errNotImplemented("BSCLR");		
	}
}
