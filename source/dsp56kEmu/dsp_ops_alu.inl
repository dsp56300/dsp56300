#pragma once

#include "dsp.h"
#include "registers.h"

namespace dsp56k
{
	constexpr int64_t g_alu_max_56		=  0x7FFFFFFFFFFFFF;
	constexpr int64_t g_alu_min_56		= -0x80000000000000;
	constexpr uint64_t g_alu_max_56_u	=  0xffffffffffffff;

	// _____________________________________________________________________________
	// alu_and
	//
	void DSP::alu_and( bool ab, TWord _val )
	{
		TReg56& d = ab ? reg.b : reg.a;

		d.var &= (TInt64(_val)<<24) | 0xFF000000FFFFFF;

		// S L E U N Z V C
		// v - - - * * * -
		sr_toggle( SR_N, bittest( d, 47 ) );
		sr_toggle( SR_Z, (d.var & 0xffffff000000) == 0 );
		sr_clear( SR_V );
		sr_s_update();
	}


	// _____________________________________________________________________________
	// alu_or
	//
	void DSP::alu_or( bool ab, TWord _val )
	{
		TReg56& d = ab ? reg.b : reg.a;

		d.var |= (TInt64(_val)<<24);

		// S L E U N Z V C
		// v - - - * * * -
		sr_toggle( SR_N, bittest( d, 47 ) );
		sr_toggle( SR_Z, (d.var & 0xffffff000000) == 0 );
		sr_clear( SR_V );
		sr_s_update();
	}

	// _____________________________________________________________________________
	// alu_add
	//
	void DSP::alu_add( bool ab, const TReg56& _val )
	{
		TReg56& d = ab ? reg.b : reg.a;

		const TReg56 old = d;

		const uint64_t d64 = d.var;
		const uint64_t res = d64 + _val.var;

		d.var = res;
		d.doMasking();

		const bool carry = res > g_alu_max_56_u;

		// S L E U N Z V C

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_l_update_by_v();
		sr_toggle(SR_C, carry);
		sr_clear(SR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops

	//	dumpCCCC();
	}

	// _____________________________________________________________________________
	// alu_cmp
	//
	void DSP::alu_cmp( bool ab, const TReg56& _val, bool _magnitude )
	{
		TReg56& d = ab ? reg.b : reg.a;

		const TReg56 oldD = d;

		uint64_t d64 = d.var;

		uint64_t val = _val.var;

		if( _magnitude )
		{
			const auto d64Signed = d.signextend<int64_t>();
			if(d64Signed < 0)
				d64 = -d64Signed;

			const auto valSigned = _val.signextend<int64_t>();
			if(valSigned < 0)
				val = -valSigned;
		}

		const auto res = static_cast<uint64_t>(d64) - static_cast<uint64_t>(val);

		const auto carry = res > 0xffffffffffffff;

		d.var = res;
		d.doMasking();

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_clear(SR_V);		// as cmp is identical to sub, the same for the V bit applies (see sub for details)
		sr_l_update_by_v();
		sr_toggle(SR_C, carry);

		d = oldD;
	}
	// _____________________________________________________________________________
	// alu_sub
	//
	void DSP::alu_sub( bool ab, const TReg56& _val )
	{
		TReg56& d = ab ? reg.b : reg.a;

		const uint64_t d64 = d.var;
		const uint64_t res = d64 - _val.var;

		const auto carry = res > 0xffffffffffffff;

		d.var = res;
		d.doMasking();

		// S L E U N Z V C
		sr_toggle(SR_C, carry);
		sr_clear(SR_V);						// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_l_update_by_v();
	}

	// _____________________________________________________________________________
	// alu_asr
	//
	void DSP::alu_asr( bool abDst, bool abSrc, int _shiftAmount )
	{
		const TReg56& dSrc = abSrc ? reg.b : reg.a;

		const TInt64 d64 = dSrc.signextend<TInt64>();

		sr_toggle( SR_C, _shiftAmount && bittest(d64,_shiftAmount-1) );

		const TInt64 res = d64 >> _shiftAmount;

		TReg56& d = abDst ? reg.b : reg.a;

		d.var = res & 0x00ffffffffffffff;

		// S L E U N Z V C

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_clear(SR_V);
		sr_l_update_by_v();
	}

	// _____________________________________________________________________________
	// alu_asl
	//
	void DSP::alu_asl( bool abDst, bool abSrc, int _shiftAmount )
	{
		const TReg56& dSrc = abSrc ? reg.b : reg.a;

		const TInt64 d64 = dSrc.signextend<TInt64>();

		sr_toggle( SR_C, _shiftAmount && ((d64 & (TInt64(1)<<(56-_shiftAmount))) != 0) );

		const TInt64 res = d64 << _shiftAmount;

		TReg56& d = abDst ? reg.b : reg.a;

		d.var = res & 0x00ffffffffffffff;

		// Overflow: Set if Bit 55 is changed any time during the shift operation, cleared otherwise.
		// What that means for us if all bits that are shifted out need to be identical to not overflow
		int64_t overflowMaskI = 0x8000000000000000;
		overflowMaskI >>= _shiftAmount;
		uint64_t overflowMaskU = overflowMaskI;
		overflowMaskU >>= 8;
		const uint64_t v = d64 & overflowMaskU;
		const bool isOverflow = v != overflowMaskU && v != 0;

		// S L E U N Z V C
		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_toggle(SR_V, isOverflow);
		sr_l_update_by_v();
	}

	// _____________________________________________________________________________
	// alu_lsl
	//
	void DSP::alu_lsl( bool ab, int _shiftAmount )
	{
		TReg24 d = ab ? b1() : a1();

		sr_toggle( SR_C, _shiftAmount && bittest( d, 23-_shiftAmount+1) );

		const int res = (d.var << _shiftAmount) & 0x00ffffff;

		if( ab )
			b1(TReg24(res));
		else
			a1(TReg24(res));

		// S L E U N Z V C

		sr_toggle( SR_N, bittest(res,23) );
		sr_toggle( SR_Z, res == 0 );
		sr_clear( SR_V );

		sr_s_update();
		sr_l_update_by_v();
	}

	// _____________________________________________________________________________
	// alu_lsr
	//
	void DSP::alu_lsr( bool ab, int _shiftAmount )
	{
		TReg24 d = ab ? b1() : a1();

		sr_toggle( SR_C, _shiftAmount && bittest( d, _shiftAmount-1) );

		const unsigned int res = ((unsigned int)d.var >> _shiftAmount);

		if( ab )
			b1(TReg24(res));
		else
			a1(TReg24(res));

		// S L E U N Z V C

		sr_toggle( SR_N, bittest(res,23) );
		sr_toggle( SR_Z, res == 0 );
		sr_clear( SR_V );

		sr_s_update();
		sr_l_update_by_v();
	}


	void DSP::alu_addr(bool ab)
	{
		auto& d			= ab ? reg.b : reg.a;
		const auto& s	= ab ? reg.a : reg.b;

		uint64_t res = static_cast<uint64_t>(d.var) >> 1;
		res |= d.var & (1ll<<55);

		res += static_cast<uint64_t>(s.var);

		const auto carry = res > g_alu_max_56_u;

		d.var = res;
		d.doMasking();

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_v_update(res, d);
		sr_l_update_by_v();
		sr_toggle(SR_C, carry);
	}

	void DSP::alu_rol(const bool ab)
	{
		auto& d = ab ? reg.b.var : reg.a.var;

		const auto c = bitvalue<uint64_t,47>(d);

		auto shifted = (d << 1) & 0x00ffffff000000;
		shifted |= sr_val(SRB_C) << 24;
		
		d &= 0xff000000ffffff;
		d |= shifted;

		sr_toggle(SRB_N, bitvalue<uint64_t, 47>(shifted));	// Set if bit 47 of the result is set
		sr_toggle(SR_Z, shifted == 0);						// Set if bits 47–24 of the result are 0
		sr_clear(SR_V);										// This bit is always cleared
		sr_toggle(SRB_C, c);								// Set if bit 47 of the destination operand is set, and cleared otherwise
	}

	void DSP::alu_addl(bool ab)
	{
		TReg56&			d = ab ? reg.b : reg.a;
		const TReg56&	s = ab ? reg.a : reg.b;

		const TReg56 old = d;
		const TInt64 res = (d.signextend<TInt64>() << 1) + s.signextend<TInt64>();
		d.var = res;
		d.doMasking();

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_clear(SR_V);		// I did not manage to make the ALU overflow in the simulator, apparently that SR bit is only used for other ops
		sr_l_update_by_v();
		sr_c_update_arithmetic(old,d);
	}

	void DSP::alu_clr(bool ab)
	{
		auto& dst = ab ? reg.b : reg.a;
		dst.var = 0;

		sr_clear( static_cast<CCRMask>(SR_E | SR_N | SR_V) );
		sr_set( static_cast<CCRMask>(SR_U | SR_Z) );
		// TODO: SR_L and SR_S are changed according to standard definition, but that should mean that no update is required?!
	}

	// _____________________________________________________________________________
	// alu_bclr
	//
	TWord DSP::alu_bclr( TWord _bit, TWord _val )
	{
		sr_toggle( SR_C, bittest(_val,_bit) );

		_val &= ~(1<<_bit);

		sr_l_update_by_v();
		sr_s_update();

		return _val;
	}

	// _____________________________________________________________________________
	// alu_mpy
	//
	void DSP::alu_mpy( bool ab, const TReg24& _s1, const TReg24& _s2, bool _negate, bool _accumulate )
	{
	//	assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

		const int64_t s1 = _s1.signextend<int64_t>();
		const int64_t s2 = _s2.signextend<int64_t>();

		auto res = s1 * s2;

		// fractional multiplication requires one post-shift to be correct
		res <<= 1;

		if( _negate )
			res = -res;

		TReg56& d = ab ? reg.b : reg.a;

		if( _accumulate )
			res += d.signextend<int64_t>();

		d.var = res & 0x00ffffffffffffff;

		// Update SR
		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_v_update(res,d);

		sr_l_update_by_v();
	}
	// _____________________________________________________________________________
	// alu_mpysuuu
	//
	void DSP::alu_mpysuuu( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _accumulate, bool _suuu )
	{
	//	assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

		TInt64 res;

		if( _suuu )
			res = TInt64( TUInt64(_s1.var) * TUInt64(_s2.var) );
		else
			res = _s1.signextend<TInt64>() * TUInt64(_s2.var);

		// fractional multiplication requires one post-shift to be correct
		res <<= 1;

		if( _negate )
			res = -res;

		TReg56& d = ab ? reg.b : reg.a;

		const TReg56 old = d;

		if( _accumulate )
			res += d.signextend<TInt64>();

		d.var = res;
		d.doMasking();

		// Update SR
		sr_e_update(d);
		sr_u_update( d );
		sr_n_update( d );
		sr_z_update( d );
		sr_v_update(res,d);

		sr_l_update_by_v();
	}
	// _____________________________________________________________________________
	// alu_dmac
	//
	void DSP::alu_dmac( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool srcUnsigned, bool dstUnsigned )
	{
		assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

		TInt64 res;

		if( srcUnsigned && dstUnsigned )	res = TInt64( TUInt64(_s1.var) * TUInt64(_s2.var) );
		else if( srcUnsigned )				res = TUInt64(_s1.var) * _s2.signextend<TInt64>();
		else if( dstUnsigned )				res = TUInt64(_s2.var) * _s1.signextend<TInt64>();
		else								res = _s2.signextend<TInt64>() * _s1.signextend<TInt64>();

		// fractional multiplication requires one post-shift to be correct
		res <<= 1;

		if( _negate )
			res = -res;

		TReg56& d = ab ? reg.b : reg.a;

		const TReg56 old = d;

		TInt64 dShifted = d.signextend<TInt64>() >> 24;

		res += dShifted;

	//	LOG( "DMAC  " << std::hex << old.var << std::hex << " + " << _s1.var << " * " << std::hex << _s2.var << " = " << std::hex << res );

		d.var = res;
		d.doMasking();

		// Update SR
		sr_e_update(d);
		sr_u_update( d );
		sr_n_update( d );
		sr_z_update( d );
		sr_v_update(res,d);

		sr_l_update_by_v();
	}

	// _____________________________________________________________________________
	// alu_mac
	//
	void DSP::alu_mac( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _uu )
	{
		assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

		TInt64 res;

		if( _uu )
			res = TInt64( TUInt64(_s1.var) * TUInt64(_s2.var) );
		else
			res = _s1.signextend<TInt64>() * TUInt64(_s2.var);

		// fractional multiplication requires one post-shift to be correct
		res <<= 1;

		if( _negate )
			res = -res;

		TReg56& d = ab ? reg.b : reg.a;

		res += d.var;

		const TReg56 old = d;

		d.var = res;

		d.doMasking();

		// Update SR
		sr_s_update();
		sr_e_update(d);
		sr_u_update( d );
		sr_n_update( d );
		sr_z_update( d );
		sr_v_update(res,d);

		sr_l_update_by_v();
	}

	// _____________________________________________________________________________
	// alu_rnd
	//
	void DSP::alu_rnd( TReg56& _alu )
	{
		int64_t rounder = 0x800000;

		if		(sr_test(SR_S1)) rounder>>=1;
		else if	(sr_test(SR_S0)) rounder<<=1;
		
		_alu.var += rounder;

		const auto mask = (rounder<<1)-1;		// all the bits to the right of, and including the rounding position

		if (!sr_test(SR_RM))					// convergent rounding. If all mask bits are cleared
		{
			if (!(_alu.var & mask)) 
				_alu.var&=~(rounder<<1);		// then the bit to the left of the rounding position is cleared in the result
		}

		_alu.var&=~mask;						// all bits to the right of and including the rounding position are cleared.

		const auto res = _alu.var;

		_alu.doMasking();

		sr_s_update();
		sr_e_update(_alu);
		sr_u_update(_alu);
		sr_n_update(_alu);
		sr_z_update(_alu);
		sr_v_update(res, _alu);

		sr_l_update_by_v();
	}
	
	inline bool DSP::alu_multiply(const TWord op)
	{
		const auto round = op & 0x1;
		const auto mulAcc = (op>>1) & 0x1;
		const auto negative = (op>>2) & 0x1;
		const auto ab = (op>>3) & 0x1;
		const auto qqq = (op>>4) & 0x7;

		TReg24 s1, s2;

		decode_QQQ_read(s1, s2, qqq);

		alu_mpy(ab, s1, s2, negative, mulAcc);

		if(round)
		{
			alu_rnd(ab);
		}

		return true;
	}

	// __________________
	//

	inline void DSP::op_Abs(const TWord op)
	{
		const auto D = getFieldValue<Abs, Field_d>(op);
		alu_abs(D);
	}

	template<TWord ab> void DSP::opCE_Abs(const TWord op)
	{
		alu_abs(ab);
	}

	inline void DSP::op_ADC(const TWord op)
	{
		errNotImplemented("ADC");
	}
	inline void DSP::op_Add_SD(const TWord op)
	{
		const auto D = getFieldValue<Add_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Add_SD, Field_JJJ>(op);
		alu_add(D, decode_JJJ_read_56(JJJ, !D));
	}
	inline void DSP::op_Add_xx(const TWord op)
	{
		const auto iiiiii	= getFieldValue<Add_xx,Field_iiiiii>(op);
		const auto ab		= getFieldValue<Add_xx,Field_d>(op);

		alu_add( ab, TReg56(iiiiii) );
	}
	inline void DSP::op_Add_xxxx(const TWord op)
	{
		const auto ab = getFieldValue<Add_xxxx,Field_d>(op);

		TReg56 r56;
		convert( r56, TReg24(immediateDataExt<Add_xxxx>()) );

		alu_add( ab, r56 );
	}
	inline void DSP::op_Addl(const TWord op)
	{
		alu_addl(getFieldValue<Addl, Field_d>(op));
	}
	inline void DSP::op_Addr(const TWord op)
	{		
		alu_addr(getFieldValue<Addr, Field_d>(op));
	}
	inline void DSP::op_And_SD(const TWord op)
	{
		const auto D = getFieldValue<And_SD, Field_d>(op);
		const auto JJ = getFieldValue<And_SD, Field_JJ>(op);
		alu_and(D, decode_JJ_read(JJ).var);
	}
	template<TWord D, TWord JJ>
	void DSP::opCE_And_SD(const TWord op)
	{
		alu_and(D ? true : false, decode_JJ_read(JJ).var);
	}
	inline void DSP::op_And_xx(const TWord op)
	{
		const auto ab		= getFieldValue<And_xx,Field_d>(op);
		const TWord xxxx	= getFieldValue<And_xx,Field_iiiiii>(op);

		alu_and(ab, xxxx );
	}
	inline void DSP::op_And_xxxx(const TWord op)
	{
		const auto ab = getFieldValue<And_xxxx,Field_d>(op);
		const TWord xxxx = immediateDataExt<And_xxxx>();

		alu_and( ab, xxxx );		
	}
	inline void DSP::op_Andi(const TWord op)
	{
		const TWord ee		= getFieldValue<Andi,Field_EE>(op);
		const TWord iiiiii	= getFieldValue<Andi,Field_iiiiiiii>(op);

		TReg8 val = decode_EE_read(ee);
		val.var &= iiiiii;
		decode_EE_write(ee,val);			
	}
	inline void DSP::op_Asl_D(const TWord op)
	{
		const auto D = getFieldValue<Asl_D, Field_d>(op);
		alu_asl(D, D, 1);
	}
	template<TWord D> void DSP::opCE_Asl_D(const TWord op)
	{
		alu_asl(D, D, 1);
	}
	inline void DSP::op_Asl_ii(const TWord op)
	{
		const TWord shiftAmount	= getFieldValue<Asl_ii,Field_iiiiii>(op);

		const bool abDst		= getFieldValue<Asl_ii,Field_D>(op);
		const bool abSrc		= getFieldValue<Asl_ii,Field_S>(op);

		alu_asl( abDst, abSrc, shiftAmount );					
	}
	inline void DSP::op_Asl_S1S2D(const TWord op)
	{
		const TWord sss = getFieldValue<Asl_S1S2D,Field_sss>(op);
		const bool abDst = getFieldValue<Asl_S1S2D,Field_D>(op);
		const bool abSrc = getFieldValue<Asl_S1S2D,Field_S>(op);

		const TWord shiftAmount = decode_sss_read<TWord>( sss );

		alu_asl( abDst, abSrc, shiftAmount );
	}
	inline void DSP::op_Asr_D(const TWord op)
	{
		const auto D = getFieldValue<Asr_D, Field_d>(op);
		alu_asr(D, D, 1);
	}
	inline void DSP::op_Asr_ii(const TWord op)
	{		
		const TWord shiftAmount	= getFieldValue<Asr_ii,Field_iiiiii>(op);

		const bool abDst		= getFieldValue<Asr_ii,Field_D>(op);
		const bool abSrc		= getFieldValue<Asr_ii,Field_S>(op);

		alu_asr( abDst, abSrc, shiftAmount );
	}
	inline void DSP::op_Asr_S1S2D(const TWord op)
	{
		const TWord sss = getFieldValue<Asr_S1S2D,Field_sss>(op);
		const bool abDst = getFieldValue<Asr_S1S2D,Field_D>(op);
		const bool abSrc = getFieldValue<Asr_S1S2D,Field_S>(op);

		const auto shiftAmount = decode_sss_read<TWord>( sss );

		alu_asr( abDst, abSrc, shiftAmount );			
	}
	inline void DSP::op_Clb(const TWord op)
	{
		errNotImplemented("CLB");
	}
	inline void DSP::op_Clr(const TWord op)
	{
		const auto D = getFieldValue<Clr, Field_d>(op);
		alu_clr(D);
	}
	inline void DSP::op_Cmp_S1S2(const TWord op)
	{
		const auto D = getFieldValue<Cmp_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmp_S1S2, Field_JJJ>(op);
		alu_cmp(D, decode_JJJ_read_56(JJJ, !D), false);
	}
	inline void DSP::op_Cmp_xxS2(const TWord op)
	{
		const TWord iiiiii = getFieldValue<Cmp_xxS2,Field_iiiiii>(op);
		
		TReg56 r56;
		convert( r56, TReg24(iiiiii) );

		alu_cmp( bittest(op,3), r56, false );
	}
	inline void DSP::op_Cmp_xxxxS2(const TWord op)
	{
		const TReg24 s( signextend<int,24>( immediateDataExt<Cmp_xxxxS2>() ) );

		TReg56 r56;
		convert( r56, s );

		alu_cmp( bittest(op,3), r56, false );
	}
	inline void DSP::op_Cmpm_S1S2(const TWord op)
	{
		const auto D = getFieldValue<Cmpm_S1S2, Field_d>(op);
		const auto JJJ = getFieldValue<Cmpm_S1S2, Field_JJJ>(op);
		alu_cmp(D, decode_JJJ_read_56(JJJ, !D), true);
	}
	inline void DSP::op_Cmpu_S1S2(const TWord op)
	{
		errNotImplemented("CMPU");		
	}
	inline void DSP::op_Dec(const TWord op)
	{
		auto& d = getFieldValue<Dec,Field_d>(op) ? reg.b : reg.a;

		const auto old = d;
		const auto res = --d.var;

		d.doMasking();

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_v_update(res,d);
		sr_l_update_by_v();
		sr_c_update_arithmetic(old,d);
		sr_toggle( SR_C, bittest(d,47) != bittest(old,47) );
	}

	inline void DSP::op_Div(const TWord op)
	{
		const TWord jj	= getFieldValue<Div,Field_JJ>(op);
		const auto ab	= getFieldValue<Div,Field_d>(op);

		auto& d = ab ? reg.b : reg.a;

		const TReg24 s24 = decode_JJ_read( jj );

		const auto msbOld = bitvalue<55>(d);
		
		const auto c = msbOld != bitvalue<23>(s24);
		
		d.var <<= 1;
		d.var |= sr_test(SR_C);

		const auto msbNew = bitvalue<55>(d);

		if( c )
			d.var = ((d.var + (signextend<TInt64,24>(s24.var) << 24) )&0x00ffffffff000000) | (d.var & 0xffffff);
		else
			d.var = ((d.var - (signextend<TInt64,24>(s24.var) << 24) )&0x00ffffffff000000) | (d.var & 0xffffff);

		sr_toggle( SRB_C, !bitvalue<55>(d) );	// Set if bit 55 of the result is cleared.
		sr_toggle( SRB_V, msbNew != msbOld );	// Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.

		if(msbNew != msbOld)
			sr_set(SR_L);						// Set if the Overflow bit (V) is set.

//		LOG( "DIV: d" << std::hex << debugOldD.var << " s" << std::hex << debugOldS.var << " =>" << std::hex << d.var );
	}
	inline void DSP::op_Dmac(const TWord op)
	{
		const auto ss			= getFieldValue<Dmac,Field_S, Field_s>(op);
		const bool ab			= getFieldValue<Dmac,Field_d>(op);
		const bool negate		= getFieldValue<Dmac,Field_k>(op);

		const TWord qqqq		= getFieldValue<Dmac,Field_QQQQ>(op);

		const bool s1Unsigned	= ss > 1;
		const bool s2Unsigned	= ss > 0;

		TReg24 s1, s2;
		decode_QQQQ_read( s1, s2, qqqq );

		// TODO: untested
		alu_dmac( ab, s1, s2, negate, s1Unsigned, s2Unsigned );
	}
	inline void DSP::op_Eor_SD(const TWord op)
	{
//		alu_eor(D, decode_JJJ_read_24(JJJ, !D).var);
		errNotImplemented("EOR");
	}
	inline void DSP::op_Eor_xx(const TWord op)
	{
		errNotImplemented("EOR");		
	}
	inline void DSP::op_Eor_xxxx(const TWord op)
	{
		errNotImplemented("EOR");
	}
	inline void DSP::op_Extract_S1S2(const TWord op)
	{
		errNotImplemented("EXTRACT");		
	}
	inline void DSP::op_Extract_CoS2(const TWord op)
	{
		errNotImplemented("EXTRACT");
	}
	inline void DSP::op_Extractu_S1S2(const TWord op)  // 00001100 00011010 100sSSSD
	{
		const TWord sss = getFieldValue<Extractu_S1S2, Field_SSS>(op);
		const TWord width_offset = decode_sss_read<TWord>(sss);

		const bool abDst = getFieldValue<Extractu_S1S2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_S1S2, Field_s>(op);

		const auto width = (width_offset >> 12) & 0x3f;
		const auto offset = width_offset & 0x3f;

		const TReg56& dSrc = abSrc ? reg.b : reg.a;
		TReg56& dDst = abDst ? reg.b : reg.a;
		const auto mask = 0xFFFFFFFFFFFFFF >> (56 - width);
		dDst.var = (dSrc.var >> offset) & mask;

		sr_clear(SR_C);
		sr_clear(SR_V);
		sr_u_update(dDst);
		sr_e_update(dDst);
		sr_n_update(dDst);
		sr_z_update(dDst);
	}
	inline void DSP::op_Extractu_CoS2(const TWord op)  // 00001100 00011000 100s000D
	{
		const TWord width_offset = fetchOpWordB();

		const bool abDst = getFieldValue<Extractu_CoS2, Field_D>(op);
		const bool abSrc = getFieldValue<Extractu_CoS2, Field_s>(op);

		const auto width = (width_offset >> 12) & 0x3f;
		const auto offset = width_offset & 0x3f;

		const TReg56& dSrc = abSrc ? reg.b : reg.a;
		TReg56& dDst = abDst ? reg.b : reg.a;
		const auto mask = 0xFFFFFFFFFFFFFF >> (56 - width);
		dDst.var = (dSrc.var >> offset) & mask;

		sr_clear(SR_C);
		sr_clear(SR_V);
		sr_u_update(dDst);
		sr_e_update(dDst);
		sr_n_update(dDst);
		sr_z_update(dDst);
	}
	inline void DSP::op_Inc(const TWord op)
	{
		auto& d = getFieldValue<Inc,Field_d>(op) ? reg.b : reg.a;

		const auto old = d;

		const auto res = ++d.var;

		d.doMasking();

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
		sr_z_update(d);
		sr_v_update(res,d);
		sr_l_update_by_v();
		sr_c_update_arithmetic(old,d);
		sr_toggle( SR_C, bittest(d,47) != bittest(old,47) );
	}
	inline void DSP::op_Insert_S1S2(const TWord op)
	{
		errNotImplemented("INSERT");
	}
	inline void DSP::op_Insert_CoS2(const TWord op)
	{
		errNotImplemented("INSERT");
	}
	inline void DSP::op_Lsl_D(const TWord op)
	{
		const auto D = getFieldValue<Lsl_D,Field_D>(op);
		alu_lsl(D, 1);
	}
	inline void DSP::op_Lsl_ii(const TWord op)
	{
		const auto shiftAmount = getFieldValue<Lsl_ii,Field_iiiii>(op);
		const auto abDst = getFieldValue<Lsl_ii,Field_D>(op);

		alu_lsl(abDst, shiftAmount);
	}
	inline void DSP::op_Lsl_SD(const TWord op)
	{
		errNotImplemented("LSL");
	}
	inline void DSP::op_Lsr_D(const TWord op)
	{
		const auto D = getFieldValue<Lsr_D,Field_D>(op);
		alu_lsr(D, 1);
	}
	inline void DSP::op_Lsr_ii(const TWord op)
	{
		const auto shiftAmount = getFieldValue<Lsr_ii,Field_iiiii>(op);
		const auto abDst = getFieldValue<Lsr_ii,Field_D>(op);

		alu_lsr(abDst, shiftAmount);
	}
	inline void DSP::op_Lsr_SD(const TWord op)
	{
		errNotImplemented("LSR");		
	}
	inline void DSP::op_Mac_S1S2(const TWord op)
	{
		alu_multiply(op);
	}
	inline void DSP::op_Mac_S(const TWord op)
	{
		const TWord sssss	= getFieldValue<Mac_S,Field_sssss>(op);
		const TWord qq		= getFieldValue<Mac_S,Field_QQ>(op);
		const bool	ab		= getFieldValue<Mac_S,Field_d>(op);
		const bool	negate	= getFieldValue<Mac_S,Field_k>(op);

		const TReg24 s1 = decode_QQ_read( qq );
		const TReg24 s2( decode_sssss(sssss) );

		alu_mac( ab, s1, s2, negate, false );		
	}
	inline void DSP::op_Maci_xxxx(const TWord op)
	{
		errNotImplemented("MACI");		
	}
	inline void DSP::op_Macsu(const TWord op)
	{
		const bool uu			= getFieldValue<Macsu,Field_s>(op);
		const bool ab			= getFieldValue<Macsu,Field_d>(op);
		const bool negate		= getFieldValue<Macsu,Field_k>(op);
		const TWord qqqq		= getFieldValue<Macsu,Field_QQQQ>(op);

		TReg24 s1, s2;
		decode_QQQQ_read( s1, s2, qqqq );

		alu_mac( ab, s1, s2, negate, uu );
	}
	inline void DSP::op_Macr_S1S2(const TWord op)
	{
		alu_multiply(op);
	}
	inline void DSP::op_Macr_S(const TWord op)
	{
		const TWord sssss	= getFieldValue<Macr_S,Field_sssss>(op);
		const bool ab		= getFieldValue<Macr_S,Field_d>(op);
		const bool negate	= getFieldValue<Macr_S,Field_k>(op);
		const TWord qq		= getFieldValue<Macr_S,Field_QQ>(op);

		const TReg24 s1 = decode_QQ_read( qq );
		const TReg24 s2( decode_sssss(sssss) );

		alu_mac( ab, s1, s2, negate, false );
		alu_rnd( ab );
	}
	inline void DSP::op_Macri_xxxx(const TWord op)
	{
		errNotImplemented("MACRI");		
	}
	inline void DSP::op_Max(const TWord op)
	{
		errNotImplemented("MAX");
	}
	inline void DSP::op_Maxm(const TWord op)
	{
		errNotImplemented("MAXM");
	}
	inline void DSP::op_Merge(const TWord op)
	{
		errNotImplemented("MERGE");		
	}
	inline void DSP::op_Mpy_S1S2D(const TWord op)
	{
		alu_multiply(op);
	}
	inline void DSP::op_Mpy_SD(const TWord op)
	{
		const int sssss		= getFieldValue<Mpy_SD,Field_sssss>(op);
		const TWord QQ		= getFieldValue<Mpy_SD,Field_QQ>(op);
		const bool ab		= getFieldValue<Mpy_SD,Field_d>(op);
		const bool negate	= getFieldValue<Mpy_SD,Field_k>(op);

		const TReg24 s1 = decode_QQ_read(QQ);
		const TReg24 s2 = TReg24( decode_sssss(sssss) );

		alu_mpy( ab, s1, s2, negate, false );
	}
	inline void DSP::op_Mpy_su(const TWord op)
	{
		const bool ab		= getFieldValue<Mpy_su,Field_d>(op);
		const bool negate	= getFieldValue<Mpy_su,Field_k>(op);
		const bool uu		= getFieldValue<Mpy_su,Field_s>(op);
		const TWord qqqq	= getFieldValue<Mpy_su,Field_QQQQ>(op);

		TReg24 s1, s2;
		decode_QQQQ_read( s1, s2, qqqq );

		alu_mpysuuu( ab, s1, s2, negate, false, uu );
	}
	inline void DSP::op_Mpyi(const TWord op)
	{
		const bool	ab		= getFieldValue<Mpyi,Field_d>(op);
		const bool	negate	= getFieldValue<Mpyi,Field_k>(op);
		const TWord qq		= getFieldValue<Mpyi,Field_qq>(op);

		const TReg24 s		= TReg24(immediateDataExt<Mpyi>());

		const TReg24 reg	= decode_qq_read(qq);

		alu_mpy( ab, reg, s, negate, false );
	}
	inline void DSP::op_Mpyr_S1S2D(const TWord op)
	{
		alu_multiply(op);
	}
	inline void DSP::op_Mpyr_SD(const TWord op)
	{
		errNotImplemented("MPYR");
	}
	inline void DSP::op_Mpyri(const TWord op)
	{
		errNotImplemented("MPYRI");
	}
	inline void DSP::op_Neg(const TWord op)
	{
		const auto D = getFieldValue<Neg, Field_d>(op);
		alu_neg(D);
	}
	inline void DSP::op_Not(const TWord op)
	{
		const auto D = getFieldValue<Not, Field_d>(op);
		alu_not(D);
	}
	inline void DSP::op_Or_SD(const TWord op)
	{
		const auto D = getFieldValue<Or_SD, Field_d>(op);
		const auto JJ = getFieldValue<Or_SD, Field_JJ>(op);
		alu_or(D, decode_JJ_read(JJ).var);
	}
	inline void DSP::op_Or_xx(const TWord op)
	{
		errNotImplemented("OR");
	}
	inline void DSP::op_Or_xxxx(const TWord op)
	{
		errNotImplemented("OR");
	}
	inline void DSP::op_Ori(const TWord op)
	{
		const TWord iiiiiiii = getFieldValue<Ori,Field_iiiiiiii>(op);
		const TWord ee = getFieldValue<Ori,Field_EE>(op);

		switch( ee )
		{
		case 0:	mr ( TReg8( mr().var | iiiiiiii) );	break;
		case 1:	ccr( TReg8(ccr().var | iiiiiiii) );	break;
		case 2:	com( TReg8(com().var | iiiiiiii) );	break;
		case 3:	eom( TReg8(eom().var | iiiiiiii) );	break;
		}
	}
	inline void DSP::op_Rnd(const TWord op)
	{
		const auto D = getFieldValue<Rnd, Field_d>(op);
		alu_rnd(D);
	}
	inline void DSP::op_Rol(const TWord op)
	{
		const auto D = getFieldValue<Rol, Field_d>(op);
		alu_rol(D);
	}
	inline void DSP::op_Ror(const TWord op)
	{
		errNotImplemented("ROR");
	}
	inline void DSP::op_Sbc(const TWord op)
	{
		errNotImplemented("SBC");
	}
	inline void DSP::op_Sub_SD(const TWord op)
	{
		const auto D = getFieldValue<Sub_SD, Field_d>(op);
		const auto JJJ = getFieldValue<Sub_SD, Field_JJJ>(op);
		alu_sub(D, decode_JJJ_read_56(JJJ, !D));
	}
	inline void DSP::op_Sub_xx(const TWord op)
	{
		const auto ab		= getFieldValue<Sub_xx,Field_d>(op);
		const TWord iiiiii	= getFieldValue<Sub_xx,Field_iiiiii>(op);

		alu_sub( ab, TReg56(iiiiii) );
	}
	inline void DSP::op_Sub_xxxx(const TWord op)
	{
		const auto ab = getFieldValue<Sub_xxxx,Field_d>(op);

		TReg56 r56;
		convert( r56, TReg24(immediateDataExt<Sub_xxxx>()) );

		alu_sub( ab, r56 );
	}
	inline void DSP::op_Subl(const TWord op)
	{
		errNotImplemented("subl");
	}
	inline void DSP::op_subr(const TWord op)
	{
		errNotImplemented("subr");
	}
	inline void DSP::op_Tfr(const TWord op)
	{
		const auto D = getFieldValue<Tfr, Field_d>(op);
		const auto JJJ = getFieldValue<Tfr, Field_JJJ>(op);
		alu_tfr(D, decode_JJJ_read_56(JJJ, !D));
	}
	inline void DSP::op_Tst(const TWord op)
	{
		const auto D = getFieldValue<Tfr, Field_d>(op);
		alu_tst(D);
	}
	inline void DSP::op_Vsl(const TWord op)
	{
		errNotImplemented("VSL");		
	}
}
