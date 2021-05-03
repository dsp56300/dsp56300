#pragma once
#include "types.h"
#include "opcodes.h"
#include "dsp.h"

namespace dsp56k
{
	template<Instruction Inst, EMemArea Area, TWord write, TWord MMM> void DSP::move_ddddd_MMMRRR(TWord op)
	{
		const TWord ddddd	= getFieldValue<Inst,Field_dd, Field_ddd>(op);

		if constexpr (write)
		{
			const auto m = readMem<Inst, MMM>(op, Area);
			decode_ddddd_write<TReg24>( ddddd, TReg24(m));
		}
		else
		{
			const auto r = decode_ddddd_read<TWord>( ddddd );
			writeMem<Inst, MMM>(op, Area, r);
		}
	}
	template<Instruction Inst, EMemArea Area, TWord write> void DSP::move_ddddd_MMMRRR(TWord op)
	{
		const TWord ddddd	= getFieldValue<Inst,Field_dd, Field_ddd>(op);

		if constexpr (write)
		{
			const auto m = readMem<Inst>(op, Area);
			decode_ddddd_write<TReg24>( ddddd, TReg24(m));
		}
		else
		{
			const auto r = decode_ddddd_read<TWord>( ddddd );
			writeMem<Inst>(op, Area, r);
		}
	}
	template<Instruction Inst> void DSP::move_L(TWord op)
	{
		const auto LLL		= getFieldValue<Inst,Field_L, Field_LL>(op);
		const auto write	= getFieldValue<Inst,Field_W>(op);

		const TWord ea = effectiveAddress<Inst>(op);

		if( write )
		{
			const TReg24 x( memRead( MemArea_X, ea ) );
			const TReg24 y( memRead( MemArea_Y, ea ) );

			decode_LLL_write( LLL, x,y );
		}
		else
		{
			TWord x,y;

			decode_LLL_read( LLL, x, y );

			memWrite( MemArea_X, ea, x );
			memWrite( MemArea_Y, ea, y );
		}
	}
	template<Instruction Inst, EMemArea Area> void DSP::move_Rnxxxx(TWord op)
	{
		const TWord DDDDDD	= getFieldValue<Inst,Field_DDDDDD>(op);
		const auto	write	= getFieldValue<Inst,Field_W>(op);
		const TWord rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = pcRelativeAddressExt<Inst>();
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		if( write )
		{
			decode_dddddd_write( DDDDDD, TReg24(memRead( Area, ea )) );
		}
		else
		{
			memWrite( Area, ea, decode_dddddd_read( DDDDDD ).var );
		}
	}

	// _______________________
	//
	
	inline void DSP::op_Move_Nop(const TWord op)
	{
	}
	inline void DSP::op_Move_xx(const TWord op)
	{
		const TWord ddddd = getFieldValue<Move_xx, Field_ddddd>(op);
		const TReg8 iiiiiiii = TReg8(static_cast<uint8_t>(getFieldValue<Move_xx, Field_iiiiiiii>(op)));

		decode_ddddd_write(ddddd, iiiiiiii);
	}
	inline void DSP::op_Mover(const TWord op)
	{
		const auto eeeee = getFieldValue<Mover,Field_eeeee>(op);
		const auto ddddd = getFieldValue<Mover,Field_ddddd>(op);

		decode_ddddd_write<TReg24>( ddddd, decode_ddddd_read<TReg24>( eeeee ) );
	}
	inline void DSP::op_Move_ea(const TWord op)
	{
		const auto mm  = getFieldValue<Move_ea, Field_MM>(op);
		const auto rrr = getFieldValue<Move_ea, Field_RRR>(op);
		decode_MMMRRR_read( mm, rrr );
	}
	
	template<TWord W, TWord MMM> void DSP::opCE_Movex_ea(const TWord op)	{ move_ddddd_MMMRRR<Movex_ea, MemArea_X, W, MMM>(op); }
	template<TWord W, TWord MMM> void DSP::opCE_Movey_ea(const TWord op)	{ move_ddddd_MMMRRR<Movey_ea, MemArea_Y, W, MMM>(op); }

	template<TWord W> void DSP::opCE_Movex_aa(const TWord op)	{ move_ddddd_MMMRRR<Movex_aa, MemArea_X, W>(op); }
	template<TWord W> void DSP::opCE_Movey_aa(const TWord op)	{ move_ddddd_MMMRRR<Movey_aa, MemArea_Y, W>(op); }

	inline void DSP::op_Movex_Rnxxxx(const TWord op)	{ move_Rnxxxx<Movex_Rnxxxx, MemArea_X>(op); }
	inline void DSP::op_Movey_Rnxxxx(const TWord op)	{ move_Rnxxxx<Movey_Rnxxxx, MemArea_Y>(op);	}

	inline void DSP::op_Movex_Rnxxx(const TWord op)
	{
		const TWord ddddd	= getFieldValue<Movex_Rnxxx,Field_DDDD>(op);
		const auto	write	= getFieldValue<Movex_Rnxxx,Field_W>(op);
		const TWord ea		= effectiveAddress<Movex_Rnxxx>(op);
		constexpr auto area	= MemArea_X;

		if( write )
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( area, ea )) );
		}
		else
		{
			memWrite( area, ea, decode_ddddd_read<TWord>( ddddd ) );
		}		
	}
	inline void DSP::op_Movey_Rnxxx(const TWord op)
	{
		// TODO: code dup op_Movex_Rnxxx
		const TWord ddddd	= getFieldValue<Movey_Rnxxx,Field_DDDD>(op);
		const auto	write	= getFieldValue<Movey_Rnxxx,Field_W>(op);
		const TWord ea		= effectiveAddress<Movey_Rnxxx>(op);
		constexpr auto area	= MemArea_Y;

		if( write )
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( area, ea )) );
		}
		else
		{
			memWrite( area, ea, decode_ddddd_read<TWord>( ddddd ) );
		}		
	}
	inline void DSP::op_Movexr_ea(const TWord op)
	{
		const TWord F		= getFieldValue<Movexr_ea,Field_F>(op);	// true:Y1, false:Y0
		const TWord ff		= getFieldValue<Movexr_ea,Field_ff>(op);
		const TWord write	= getFieldValue<Movexr_ea,Field_W>(op);
		const TWord d		= getFieldValue<Movexr_ea,Field_d>(op);

		// S2 D2 move
		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		if(F)		y1(ab);
		else		y0(ab);
		
		// S1/D1 move
		if( write )
		{
			const auto m = readMem<Movexr_ea>(op, MemArea_X);

			decode_ee_write( ff, TReg24(m) );
		}
		else
		{
			const auto ea = effectiveAddress<Movexr_ea>(op);
			memWrite( MemArea_X, ea, decode_ff_read(ff).toWord());
		}
	}
	inline void DSP::op_Movexr_A(const TWord op)
	{
		const TWord d		= getFieldValue<Movexr_A,Field_d>(op);
		const TWord ea		= effectiveAddress<Movexr_A>(op);

		// S1/D1 move
		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		memWrite(MemArea_X, ea, ab.var);

		// S2/D2 move
		if(d)
			setB(x0());
		else
			setA(x0());
	}

	inline void DSP::op_Moveyr_ea(const TWord op)
	{
		const bool e		= getFieldValue<Moveyr_ea,Field_e>(op);	// true:X1, false:X0
		const TWord ff		= getFieldValue<Moveyr_ea,Field_ff>(op);
		const bool write	= getFieldValue<Moveyr_ea,Field_W>(op);
		const bool d		= getFieldValue<Moveyr_ea,Field_d>(op);

		// S2 D2 move
		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		if( e )		x1(ab);
		else		x0(ab);
	
		// S1/D1 move
		if( write )
		{
			const auto m = readMem<Moveyr_ea>(op, MemArea_Y);
			decode_ff_write( ff, TReg24(m) );
		}
		else
		{
			const TWord ea = effectiveAddress<Moveyr_ea>(op);
			memWrite( MemArea_Y, ea, decode_ff_read( ff ).toWord() );
		}
	}
	inline void DSP::op_Moveyr_A(const TWord op)
	{
		const TWord d		= getFieldValue<Moveyr_A,Field_d>(op);
		const TWord ea 		= effectiveAddress<Moveyr_A>(op);

		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		memWrite( MemArea_Y, ea, ab.var );
		if (d)
			setB(y0());
		else
			setA(y0());
	}

	inline void DSP::op_Movel_ea(const TWord op)	{ move_L<Movel_ea>(op); }
	inline void DSP::op_Movel_aa(const TWord op)	{ move_L<Movel_aa>(op); }

	inline void DSP::op_Movexy(const TWord op)
	{
		assert(false);
		/*
		const TWord mmrrr	= getFieldValue<Movexy,Field_MM, Field_RRR>(op);
		const TWord mmrr	= getFieldValue<Movexy,Field_mm, Field_rr>(op);
		const TWord writeX	= getFieldValue<Movexy,Field_W>(op);
		const TWord	writeY	= getFieldValue<Movexy,Field_w>(op);
		const TWord	ee		= getFieldValue<Movexy,Field_ee>(op);
		const TWord ff		= getFieldValue<Movexy,Field_ff>(op);

		const TWord eaX				= decode_XMove_MMRRR( mmrrr );
		const TWord regIdxOffset	= ((mmrrr&0x7) >= 4) ? 0 : 4;
		const TWord eaY				= decode_YMove_mmrr( mmrr, regIdxOffset );

		if(!writeX)		memWrite( MemArea_X, eaX, decode_ee_read( ee ).toWord() );
		if(!writeY)		memWrite( MemArea_Y, eaY, decode_ff_read( ff ).toWord() );

		if( writeX )	decode_ee_write( ee, TReg24(memRead( MemArea_X, eaX )) );
		if( writeY )	decode_ff_write( ff, TReg24(memRead( MemArea_Y, eaY )) );
		*/
	}

	template<TWord W, TWord w, TWord ee, TWord ff>
	void DSP::opCE_Movexy(const TWord op)
	{
		const TWord MM		= getFieldValue<Movexy, Field_MM>(op);
		const TWord RRR		= getFieldValue<Movexy, Field_RRR>(op);
		const TWord mm		= getFieldValue<Movexy, Field_mm>(op);
		const TWord rr		= getFieldValue<Movexy, Field_rr>(op);

		constexpr TWord writeX	= W;
		constexpr TWord	writeY	= w;

		const TWord eaX				= decode_XMove_MMRRR( MM, RRR );
		const TWord regIdxOffset	= RRR >= 4 ? 0 : 4;
		const TWord eaY				= decode_XMove_MMRRR( mm, (rr + regIdxOffset) & 7 );

		if constexpr(!writeX)	memWrite( MemArea_X, eaX, decode_ee_read<ee>().toWord() );
		if constexpr(!writeY)	memWrite( MemArea_Y, eaY, decode_ff_read<ff>().toWord() );

		if constexpr( writeX )	decode_ee_write<ee>(TReg24(memRead( MemArea_X, eaX )) );
		if constexpr( writeY )	decode_ff_write<ff>(TReg24(memRead( MemArea_Y, eaY )) );
	}

	inline void DSP::op_Movec_ea(const TWord op)
	{
		const TWord ddddd	= getFieldValue<Movec_ea,Field_DDDDD>(op);
		const auto write	= getFieldValue<Movec_ea,Field_W>(op);

		if( write )
		{
			const auto m = readMem<Movec_ea>(op);

			decode_ddddd_pcr_write( ddddd, TReg24(m) );
		}
		else
		{
			const auto addr = effectiveAddress<Movec_ea>(op);
			const EMemArea area = getFieldValueMemArea<Movec_ea>(op);

			const auto regVal = decode_ddddd_pcr_read(ddddd);

			memWrite( area, addr, regVal.toWord() );
		}
	}
	inline void DSP::op_Movec_aa(const TWord op)
	{
		const TWord ddddd	= getFieldValue<Movec_aa,Field_DDDDD>(op);
		const auto write	= getFieldValue<Movec_aa,Field_W>(op);
		
		if( write )
		{
			const auto m = readMem<Movec_aa>(op);
			decode_ddddd_pcr_write( ddddd, TReg24(m) );
		}
		else
		{
			const TWord aaaaaa	= getFieldValue<Movec_aa,Field_aaaaaa>(op);
			const TWord addr = aaaaaa;

			const EMemArea area = getFieldValueMemArea<Movec_aa>(op);

			memWrite( area, addr, decode_ddddd_pcr_read(ddddd).toWord() );
		}		
	}
	inline void DSP::op_Movec_S1D2(const TWord op)
	{
		const auto write = getFieldValue<Movec_S1D2,Field_W>(op);

		const TWord eeeeee	= getFieldValue<Movec_S1D2,Field_eeeeee>(op);
		const TWord ddddd	= getFieldValue<Movec_S1D2,Field_DDDDD>(op);

		if( write )
			decode_ddddd_pcr_write( ddddd, decode_dddddd_read( eeeeee ) );
		else
			decode_dddddd_write( eeeeee, decode_ddddd_pcr_read( ddddd ) );
	}
	inline void DSP::op_Movec_xx(const TWord op)
	{
		const TWord iiiiiiii	= getFieldValue<Movec_xx, Field_iiiiiiii>(op);
		const TWord ddddd		= getFieldValue<Movec_xx,Field_DDDDD>(op);
		decode_ddddd_pcr_write( ddddd, TReg24(iiiiiiii) );
	}
	inline void DSP::op_Movem_ea(const TWord op)
	{
		const auto	write	= getFieldValue<Movem_ea,Field_W>(op);
		const TWord dddddd	= getFieldValue<Movem_ea,Field_dddddd>(op);

		if( write )
		{
			const auto m = readMem<Movem_ea>(op, MemArea_P);
			decode_dddddd_write( dddddd, TReg24(m));
		}
		else
		{
			const TWord ea = effectiveAddress<Movem_ea>(op);
			memWrite( MemArea_P, ea, decode_dddddd_read(dddddd).toWord() );
		}
	}
	inline void DSP::op_Movem_aa(const TWord op)
	{
		errNotImplemented("MOVE(M) S,P:aa");
	}
	inline void DSP::op_Movep_ppea(const TWord op)
	{
		const TWord pp		= getFieldValue<Movep_ppea,Field_pppppp>(op);
		const TWord mmmrrr	= getFieldValue<Movep_ppea,Field_MMM, Field_RRR>(op);
		const auto write	= getFieldValue<Movep_ppea,Field_W>(op);
		const EMemArea s	= getFieldValue<Movep_ppea,Field_s>(op) ? MemArea_Y : MemArea_X;
		const EMemArea S	= getFieldValueMemArea<Movep_ppea>(op);

		const TWord ea		= effectiveAddress<Movep_ppea>(op);

		if( write )
		{
			// TODO: remove the if here, use helper templates instead
			if( mmmrrr == MMMRRR_ImmediateData )
				memWritePeriphFFFFC0( S, pp, ea );
			else
				memWritePeriphFFFFC0( S, pp, memRead( s, ea ) );
		}
		else
			memWrite( S, ea, memReadPeriphFFFFC0( s, pp ) );
	}
	inline void DSP::op_Movep_Xqqea(const TWord op)
	{
		const auto qAddr	= getFieldValue<Movep_Xqqea,Field_qqqqqq>(op);
		const auto write	= getFieldValue<Movep_Xqqea,Field_W>(op);

		const auto area = MemArea_X;

		if( write )
		{
			const auto m = readMem<Movep_Xqqea>(op);
			memWritePeriphFFFF80( area, qAddr, m );
		}
		else
		{
			writeMem<Movep_Xqqea>(op, memReadPeriphFFFF80( area, qAddr ));
		}
	}
	inline void DSP::op_Movep_Yqqea(const TWord op)
	{
		// TODO: code dup op_Movep_Xqqea
		const auto qAddr	= getFieldValue<Movep_Yqqea,Field_qqqqqq>(op);
		const auto write	= getFieldValue<Movep_Yqqea,Field_W>(op);

		const auto area = MemArea_Y;

		if( write )
		{
			const auto m = readMem<Movep_Yqqea>(op);
			memWritePeriphFFFF80( area, qAddr, m );
		}
		else
		{
			writeMem<Movep_Yqqea>(op, memReadPeriphFFFF80( area, qAddr ));
		}
	}
	inline void DSP::op_Movep_eapp(const TWord op)
	{
		errNotImplemented("MOVE");
	}
	inline void DSP::op_Movep_eaqq(const TWord op)
	{
		errNotImplemented("MOVE");
	}
	inline void DSP::op_Movep_Spp(const TWord op)
	{
		const TWord pppppp	= getFieldValue<Movep_Spp,Field_pppppp>(op);
		const TWord dddddd	= getFieldValue<Movep_Spp,Field_dddddd>(op);
		const EMemArea area = getFieldValue<Movep_Spp,Field_s>(op) ? MemArea_Y : MemArea_X;
		const auto	write	= getFieldValue<Movep_Spp,Field_W>(op);

		if( write )
			memWritePeriphFFFFC0( area, pppppp, decode_dddddd_read( dddddd ).toWord() );
		else
			decode_dddddd_write( dddddd, TReg24(memReadPeriphFFFFC0( area, pppppp )) );
	}
	inline void DSP::op_Movep_SXqq(const TWord op)
	{
		const TWord addr	= getFieldValue<Movep_SXqq,Field_q, Field_qqqqq>(op);
		const TWord dddddd	= getFieldValue<Movep_SXqq,Field_dddddd>(op);
		const auto	write	= getFieldValue<Movep_SXqq,Field_W>(op);

		const auto area = MemArea_X;

		if( write )
			memWritePeriphFFFF80( area, addr, decode_dddddd_read( dddddd ).toWord() );
		else
			decode_dddddd_write( dddddd, TReg24(memReadPeriphFFFF80( area, addr )) );
	}
	inline void DSP::op_Movep_SYqq(const TWord op)
	{
		// TODO: code dup op_Movep_SXqq
		const TWord addr	= getFieldValue<Movep_SXqq,Field_q, Field_qqqqq>(op);
		const TWord dddddd	= getFieldValue<Movep_SXqq,Field_dddddd>(op);
		const auto	write	= getFieldValue<Movep_SXqq,Field_W>(op);

		const auto area = MemArea_Y;

		if( write )
			memWritePeriphFFFF80( area, addr, decode_dddddd_read( dddddd ).toWord() );
		else
			decode_dddddd_write( dddddd, TReg24(memReadPeriphFFFF80( area, addr )) );
	}	
}
