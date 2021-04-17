#pragma once

#include "types.h"

namespace dsp56k
{
	inline void DSP::op_Abs(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_ADC(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);		
	}
	inline void DSP::op_Add_SD(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Add_xx(const TWord op)
	{
		const TWord iiiiii	= getFieldValue<Add_xx,Field_iiiiii>(op);
		const auto ab		= getFieldValue<Add_xx,Field_d>(op);
		alu_add( ab, TReg56(iiiiii) );
	}
	inline void DSP::op_Add_xxxx(const TWord op)
	{
		const auto ab = getFieldValue<Add_xxxx,Field_d>(op);

		TReg56 r56;
		convert( r56, TReg24(fetchOpWordB()) );

		alu_add( ab, r56 );
	}
	inline void DSP::op_Addl(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Addr(const TWord op)
	{		
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_And_SD(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);		
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
		const TWord xxxx = fetchOpWordB();

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
		exec_parallel_alu_nonMultiply(op);
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
		exec_parallel_alu_nonMultiply(op);
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

		const TWord shiftAmount = decode_sss_read<TWord>( sss );

		alu_asr( abDst, abSrc, shiftAmount );			
	}
	inline void DSP::op_Bcc_Rn(const TWord op)	// 00001101 00011RRR 0100CCCC
	{
		errNotImplemented("Bcc Rn");	
	}
	inline void DSP::op_Bchg_ea(const TWord op)
	{
		errNotImplemented("BCHG");
	}
	inline void DSP::op_Bchg_aa(const TWord op)
	{
		errNotImplemented("BCHG");
	}
	inline void DSP::op_Bchg_pp(const TWord op)
	{
		errNotImplemented("BCHG");
	}
	inline void DSP::op_Bchg_qq(const TWord op)
	{
		errNotImplemented("BCHG");
	}
	inline void DSP::op_Bchg_D(const TWord op)
	{
		const TWord bit		= getFieldValue<Bchg_D,Field_bbbbb>(op);
		const TWord dddddd	= getFieldValue<Bchg_D,Field_DDDDDD>(op);

		TReg24 val = decode_dddddd_read( dddddd );

		sr_toggle( SR_C, bittestandchange( val, bit ) );

		decode_dddddd_write( dddddd, val );

		sr_s_update();
		sr_l_update_by_v();
	}
	inline void DSP::op_Bclr_ea(const TWord op)	// 0000101001MMMRRR0S0bbbbb
	{
		const TWord mmmrrr = getFieldValue<Bclr_ea,Field_MMM, Field_RRR>(op);
		const EMemArea S = getFieldValueMemArea<Bclr_ea>(op);

		const TWord bbbbb = getFieldValue<Bclr_qq,Field_bbbbb>(op);

		const TWord ea = decode_MMMRRR_read(mmmrrr);

		
		if(ea >= XIO_Reserved_High_First)
		{
			auto mem = memReadPeriph( S, ea );
			mem = alu_bclr( bbbbb, mem );
			memWritePeriph( S, ea, mem);
		}
		else
		{
			auto mem = memRead( S, ea );
			mem = alu_bclr( bbbbb, mem );
			memWrite( S, ea, mem);
		}

	}
	inline void DSP::op_Bclr_aa(const TWord op)
	{
		errNotImplemented("BCLR");
	}
	inline void DSP::op_Bclr_pp(const TWord op)
	{
		errNotImplemented("BCLR");
	}
	inline void DSP::op_Bclr_qq(const TWord op)
	{
		const TWord bit = getFieldValue<Bclr_qq,Field_bbbbb>(op);
		const TWord ea	= getFieldValue<Bclr_qq,Field_qqqqqq>(op);

		const EMemArea S = getFieldValueMemArea<Bclr_qq>(op);

		const TWord res = alu_bclr( bit, memReadPeriphFFFF80( S, ea ) );

		memWritePeriphFFFF80( S, ea, res );			
	}
	inline void DSP::op_Bclr_D(const TWord op)
	{
		const TWord bit		= getFieldValue<Bclr_D,Field_bbbbb>(op);
		const TWord dddddd	= getFieldValue<Bclr_D,Field_DDDDDD>(op);

		TWord val;
		convert( val, decode_dddddd_read( dddddd ) );

		const TWord newVal = alu_bclr( bit, val );
		decode_dddddd_write( dddddd, TReg24(newVal) );			
	}

	inline void DSP::op_BRKcc(const TWord op)
	{
		errNotImplemented("BRKcc");
	}

	inline void DSP::op_Bset_ea(const TWord op)
	{
		const TWord bit		= getFieldValue<Bset_ea,Field_bbbbb>(op);
		const TWord mmmrrr	= getFieldValue<Bset_ea,Field_MMM, Field_RRR>(op);
		const EMemArea S	= getFieldValueMemArea<Bset_ea>(op);

		const TWord ea		= decode_MMMRRR_read(mmmrrr);

		if(ea >= XIO_Reserved_High_First)
		{
			// god WHY is this even possible! Bset_pp/qq are for peripherals and even save one word!	TODO: code optimizer? We could rewrite as Bset_qq/pp + one nop
			auto val = memReadPeriph( S, ea );

			sr_toggle( SR_C, bittestandset( val, bit ) );

			memWritePeriph( S, ea, val );
		}
		else
		{
			auto val = memRead( S, ea );

			sr_toggle( SR_C, bittestandset( val, bit ) );

			memWrite( S, ea, val );
		}
	}
	inline void DSP::op_Bset_aa(const TWord op)
	{
		errNotImplemented("BSET");
	}
	inline void DSP::op_Bset_pp(const TWord op)	// 0000101010pppppp0S1bbbbb
	{
		const TWord bit		= getFieldValue<Bset_pp,Field_bbbbb>(op);
		const TWord pppppp	= getFieldValue<Bset_pp,Field_pppppp>(op);
		const EMemArea S	= getFieldValueMemArea<Bset_pp>(op);

		const TWord ea		= pppppp;

		TWord val = memReadPeriphFFFFC0( S, ea );

		sr_toggle( SR_C, bittestandset( val, bit ) );

		memWritePeriphFFFFC0( S, ea, val );
	}
	inline void DSP::op_Bset_qq(const TWord op)
	{
		const TWord bit		= getFieldValue<Bset_qq,Field_bbbbb>(op);
		const TWord qqqqqq	= getFieldValue<Bset_qq,Field_qqqqqq>(op);
		const EMemArea S	= getFieldValueMemArea<Bset_qq>(op);

		const TWord ea		= qqqqqq;

		TWord val = memReadPeriphFFFF80( S, ea );

		sr_toggle( SR_C, bittestandset( val, bit ) );

		memWritePeriphFFFF80( S, ea, val );
	}
	inline void DSP::op_Bset_D(const TWord op)
	{
		const TWord bit	= getFieldValue<Bset_D,Field_bbbbb>(op);
		const TWord d	= getFieldValue<Bset_D,Field_DDDDDD>(op);

		TReg24 val = decode_dddddd_read(d);

		if( (d & 0x3f) == 0x39 )	// is SR the destination?	TODO: magic value
		{
			bittestandset( val.var, bit );
		}
		else
		{
			sr_toggle( SR_C, bittestandset( val, bit ) );
		}

		decode_dddddd_write( d, val );

		sr_s_update();
		sr_l_update_by_v();
	}
	inline void DSP::op_Bsr_xxxx(const TWord op)
	{
		const int displacement = signextend<int,24>(fetchOpWordB());
		jsr(pcCurrentInstruction + displacement);
	}
	inline void DSP::op_Bsr_xxx(const TWord op)
	{
		const TWord aaaaaaaaa = getFieldValue<Bsr_xxx,Field_aaaa, Field_aaaaa>(op);
		const int displacement = signextend<int,9>(aaaaaaaaa);
		jsr(pcCurrentInstruction + displacement);		
	}
	inline void DSP::op_Bsr_Rn(const TWord op)
	{
        const auto rrr = getFieldValue<Bsr_Rn,Field_RRR>(op);
        jsr(pcCurrentInstruction + reg.r[rrr].var);		
	}
	inline void DSP::op_Bsset_ea(const TWord op)
	{
		errNotImplemented("BSSET");
	}
	inline void DSP::op_Bsset_aa(const TWord op)
	{
		errNotImplemented("BSSET");		
	}
	inline void DSP::op_Bsset_pp(const TWord op)
	{
		errNotImplemented("BSSET");
	}
	inline void DSP::op_Bsset_qq(const TWord op)
	{
		errNotImplemented("BSSET");		
	}
	inline void DSP::op_Bsset_S(const TWord op)
	{
		errNotImplemented("BSSET");
	}
	inline void DSP::op_Btst_ea(const TWord op)
	{
		const TWord bit = getFieldValue<Btst_ea,Field_bbbbb>(op);
		const TWord mmmrrr	= getFieldValue<Btst_ea,Field_MMM, Field_RRR>(op);
		const TWord ea = decode_MMMRRR_read( mmmrrr );
		const EMemArea S = getFieldValueMemArea<Btst_ea>(op);

		if (ea >= XIO_Reserved_High_First)
		{
			const TWord val = memReadPeriph(S, ea);
			sr_toggle(SR_C, bittest(val, bit));
		}
		else
		{
			const TWord val = memRead(S, ea);
			sr_toggle(SR_C, bittest(val, bit));
		}
		sr_s_update();
		sr_l_update_by_v();
	}
	inline void DSP::op_Btst_aa(const TWord op)
	{
		errNotImplemented("BTST aa");
	}
	inline void DSP::op_Btst_pp(const TWord op)
	{
		const TWord bitNum	= getFieldValue<Btst_pp,Field_bbbbb>(op);
		const TWord pppppp	= getFieldValue<Btst_pp,Field_pppppp>(op);
		const EMemArea S	= getFieldValueMemArea<Btst_pp>(op);

		const TWord memVal	= memReadPeriphFFFFC0( S, pppppp );

		const bool bitSet	= ( memVal & (1<<bitNum)) != 0;

		sr_toggle( SR_C, bitSet );
	}
	inline void DSP::op_Btst_qq(const TWord op)
	{
		errNotImplemented("BTST qq");
	}
	inline void DSP::op_Btst_D(const TWord op)
	{
		const TWord dddddd	= getFieldValue<Btst_D,Field_DDDDDD>(op);
		const TWord bit		= getFieldValue<Btst_D,Field_bbbbb>(op);

		TReg24 val = decode_dddddd_read( dddddd );

		sr_toggle( SR_C, bittest( val.var, bit ) );
	}
	inline void DSP::op_Clb(const TWord op)
	{
		errNotImplemented("CLB");		
	}
	inline void DSP::op_Clr(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Cmp_S1S2(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);		
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
		const TReg24 s( signextend<int,24>( fetchOpWordB() ) );

		TReg56 r56;
		convert( r56, s );

		alu_cmp( bittest(op,3), r56, false );
	}
	inline void DSP::op_Cmpm_S1S2(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Cmpu_S1S2(const TWord op)
	{
		errNotImplemented("CMPU");		
	}
	inline void DSP::op_Debug(const TWord op)
	{
		LOG( "Entering DEBUG mode" );
		errNotImplemented("DEBUG");		
	}
	inline void DSP::op_Debugcc(const TWord op)
	{
		const TWord cccc = getFieldValue<Debugcc,Field_CCCC>(op);
		if( decode_cccc( cccc ) )
		{
			LOG( "Entering DEBUG mode because condition is met" );
			errNotImplemented("DEBUGcc");
		}
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

		const auto msbOld = bittest(d,55);
		
		const bool c = msbOld != bittest(s24,23);
		
		d.var <<= 1;

		const auto msbNew = bittest(d,55);

		if( sr_test(SR_C) )
			bittestandset( d.var, 0 );
		else
			bittestandclear( d.var, 0 );

		if( c )
			d.var = ((d.var + (signextend<TInt64,24>(s24.var) << 24) )&0xffffffffff000000) | (d.var & 0xffffff);
		else
			d.var = ((d.var - (signextend<TInt64,24>(s24.var) << 24) )&0xffffffffff000000) | (d.var & 0xffffff);

		sr_toggle( SR_C, bittest(d,55) == 0 );	// Set if bit 55 of the result is cleared.
		sr_toggle( SR_V, msbNew != msbOld );	// Set if the MSB of the destination operand is changed as a result of the instructions left shift operation.
		sr_toggle( SR_L, msbNew != msbOld );	// Set if the Overflow bit (V) is set.

		d.var &= 0x00ffffffffffffff;

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
	inline void DSP::op_Do_ea(const TWord op)
	{
		const auto addr = absAddressExt<Do_ea>();
		const auto loopCount = effectiveAddress<Do_ea>(op);
		do_exec(loopCount, addr);
	}
	inline void DSP::op_Do_aa(const TWord op)
	{
		const auto addr = absAddressExt<Do_aa>();
		const auto loopCount = effectiveAddress<Do_aa>(op);
		do_exec(loopCount, addr);
	}
	inline void DSP::op_Do_xxx(const TWord op)
	{
		const TWord addr = fetchOpWordB();
		const TWord loopcount = getFieldValue<Do_xxx,Field_hhhh, Field_iiiiiiii>(op);

		do_exec( loopcount, addr );
	}
	inline void DSP::op_Do_S(const TWord op)
	{
		const TWord addr = fetchOpWordB();

		const TWord dddddd = getFieldValue<Do_S,Field_DDDDDD>(op);

		const TReg24 loopcount = decode_dddddd_read( dddddd );

		do_exec( loopcount.var, addr );
	}
	inline void DSP::op_DoForever(const TWord op)
	{
		errNotImplemented("DO FOREVER");
	}
	inline void DSP::op_Dor_ea(const TWord op)
	{
		errNotImplemented("DOR");		
	}
	inline void DSP::op_Dor_aa(const TWord op)
	{
		errNotImplemented("DOR");		
	}
	inline void DSP::op_Dor_xxx(const TWord op)
	{
        const auto loopcount = getFieldValue<Dor_xxx,Field_hhhh, Field_iiiiiiii>(op);
        const auto displacement = signextend<int, 24>(fetchOpWordB());
        do_exec(loopcount, pcCurrentInstruction + displacement);
	}
	inline void DSP::op_Dor_S(const TWord op)
	{
		const TWord dddddd = getFieldValue<Dor_S,Field_DDDDDD>(op);
		const TReg24 lc		= decode_dddddd_read( dddddd );
		
		const int displacement = signextend<int,24>(fetchOpWordB());
		do_exec( lc.var, pcCurrentInstruction + displacement);
	}
	inline void DSP::op_DorForever(const TWord op)
	{
		errNotImplemented("DOR FOREVER");		
	}
	inline void DSP::op_Enddo(const TWord op)
	{
		// TODO: do_exit?
		// restore previous loop flag
		sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

		// decrement SP twice, restoring old loop settings
		decSP();

		reg.lc = ssl();
		reg.la = ssh();	
	}
	inline void DSP::op_Eor_SD(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
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
	}
	inline void DSP::op_Ifcc(const TWord op)
	{
		const auto cccc = getFieldValue<Ifcc_U, Field_CCCC>(op);
		if( decode_cccc( cccc ) )
		{
			const auto backupCCR = ccr();
			const auto res = exec_parallel_alu(op);
			ccr(backupCCR);
		}
	}
	inline void DSP::op_Ifcc_U(const TWord op)
	{
		const auto cccc = getFieldValue<Ifcc_U, Field_CCCC>(op);
		if( decode_cccc( cccc ) )
			exec_parallel_alu(op);
	}
	inline void DSP::op_Illegal(const TWord op)
	{
		errNotImplemented("ILLEGAL");
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
	inline void DSP::op_Lra_Rn(const TWord op)
	{
		errNotImplemented("LRA");
	}
	inline void DSP::op_Lra_xxxx(const TWord op)	// 0000010001oooooo010ddddd
	{
		const TWord ddddd = getFieldValue<Lra_xxxx, Field_ddddd>(op);

		const auto ea = pcCurrentInstruction + fetchOpWordB();
		decode_ddddd_write(ddddd, TReg24(ea));
	}
	inline void DSP::op_Lsl_D(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
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
		exec_parallel_alu_nonMultiply(op);
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
	inline void DSP::op_Lua_ea(const TWord op)
	{
		const TWord mmrrr	= getFieldValue<Lua_ea,Field_MM, Field_RRR>(op);
		const TWord ddddd	= getFieldValue<Lua_ea,Field_ddddd>(op);

		const unsigned int regIdx = mmrrr & 0x07;

		const TReg24	_n = reg.n[regIdx];
		TWord			_r = reg.r[regIdx].var;
		const TReg24	_m = reg.m[regIdx];

		switch( mmrrr & 0x18 )
		{
		case 0x00:	/* 00 */	AGU::updateAddressRegister( _r, -_n.var, _m.var );		break;
		case 0x08:	/* 01 */	AGU::updateAddressRegister( _r, +_n.var, _m.var );		break;
		case 0x10:	/* 10 */	AGU::updateAddressRegister( _r, -1, _m.var );			break;
		case 0x18:	/* 11 */	AGU::updateAddressRegister( _r, +1, _m.var );			break;
		default:
			assert(0 && "impossible to happen" );
		}

		decode_ddddd_write<TReg24>( ddddd, TReg24(_r) );
	}
	inline void DSP::op_Lua_Rn(const TWord op)
	{
		const TWord dddd	= getFieldValue<Lua_Rn,Field_dddd>(op);
		const TWord a		= getFieldValue<Lua_Rn,Field_aaa, Field_aaaa>(op);
		const TWord rrr		= getFieldValue<Lua_Rn,Field_RRR>(op);

		const TReg24 _r = reg.r[rrr];

		const int aSigned = signextend<int,7>(a);

		const TReg24 val = TReg24(_r.var + aSigned);

		if( dddd < 8 )									// r0-r7
		{
			convert(reg.r[dddd],val);
		}
		else
		{
			convert(reg.n[dddd&0x07],val);
		}
	}
	inline void DSP::op_Mac_S1S2(const TWord op)
	{
		exec_parallel_alu_multiply(op);
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
		exec_parallel_alu_multiply(op);
	}
	inline void DSP::op_Macr_S(const TWord op)
	{
		errNotImplemented("MACR");		
	}
	inline void DSP::op_Macri_xxxx(const TWord op)
	{
		errNotImplemented("MACRI");		
	}
	inline void DSP::op_Max(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Maxm(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Merge(const TWord op)
	{
		errNotImplemented("MERGE");		
	}
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

		// TODO: we need to determine the two register types and call different template versions of read and write
		decode_ddddd_write<TReg24>( ddddd, decode_ddddd_read<TReg24>( eeeee ) );
	}
	inline void DSP::op_Move_ea(const TWord op)
	{
		const TWord mmrrr = getFieldValue<Move_ea, Field_MM, Field_RRR>(op);
		decode_MMMRRR_read( mmrrr );
	}
	inline void DSP::op_Movex_ea(const TWord op)
	{
		const TWord mmmrrr	= getFieldValue<Movex_ea,Field_MMM, Field_RRR>(op);
		const TWord ddddd	= getFieldValue<Movex_ea,Field_dd, Field_ddd>(op);
		const TWord write	= getFieldValue<Movex_ea,Field_W>(op);

		exec_move_ddddd_MMMRRR( ddddd, mmmrrr, write, MemArea_X);
	}
	inline void DSP::op_Movey_ea(const TWord op)
	{
		const TWord mmmrrr	= getFieldValue<Movey_ea,Field_MMM, Field_RRR>(op);
		const TWord ddddd	= getFieldValue<Movey_ea,Field_dd, Field_ddd>(op);
		const TWord write	= getFieldValue<Movey_ea,Field_W>(op);

		exec_move_ddddd_MMMRRR( ddddd, mmmrrr, write, MemArea_Y);
	}
	inline void DSP::op_Movex_aa(const TWord op)
	{
		const bool write	= getFieldValue<Movex_aa,Field_W>(op);
		const TWord ddddd	= getFieldValue<Movex_aa,Field_dd, Field_ddd>(op);
		const TWord aaaaaa	= getFieldValue<Movex_aa,Field_aaaaaa>(op);
		const EMemArea area	= MemArea_X;

		if (write)
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead(area, aaaaaa)) );
		}
		else
		{
			memWrite( area, aaaaaa, decode_ddddd_read<TWord>(ddddd) );
		}		
	}
	inline void DSP::op_Movey_aa(const TWord op)
	{
		// TODO: code dup op_Movex_aa
		const bool write	= getFieldValue<Movex_aa,Field_W>(op);
		const TWord ddddd	= getFieldValue<Movex_aa,Field_dd, Field_ddd>(op);
		const TWord aaaaaa	= getFieldValue<Movex_aa,Field_aaaaaa>(op);
		const EMemArea area	= MemArea_Y;

		if (write)
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead(area, aaaaaa)) );
		}
		else
		{
			memWrite( area, aaaaaa, decode_ddddd_read<TWord>(ddddd) );
		}
	}
	inline void DSP::op_Movex_Rnxxxx(const TWord op)
	{
		const TWord DDDDDD	= getFieldValue<Movex_Rnxxxx,Field_DDDDDD>(op);
		const auto	write	= getFieldValue<Movex_Rnxxxx,Field_W>(op);
		const TWord rrr		= getFieldValue<Movex_Rnxxxx,Field_RRR>(op);

		const int shortDisplacement = signextend<int,24>(fetchOpWordB());
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		const auto area = MemArea_X;

		if( write )
		{
			decode_dddddd_write( DDDDDD, TReg24(memRead( area, ea )) );
		}
		else
		{
			memWrite( area, ea, decode_dddddd_read( DDDDDD ).var );
		}
	}
	inline void DSP::op_Movey_Rnxxxx(const TWord op)
	{
		// TODO: code dup op_Movex_Rnxxxx
		const TWord DDDDDD	= getFieldValue<Movey_Rnxxxx,Field_DDDDDD>(op);
		const auto	write	= getFieldValue<Movey_Rnxxxx,Field_W>(op);
		const TWord rrr		= getFieldValue<Movey_Rnxxxx,Field_RRR>(op);

		const int shortDisplacement = signextend<int,24>(fetchOpWordB());
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		const auto area = MemArea_Y;

		if( write )
		{
			decode_dddddd_write( DDDDDD, TReg24(memRead( area, ea )) );
		}
		else
		{
			memWrite( area, ea, decode_dddddd_read( DDDDDD ).var );
		}
	}
	inline void DSP::op_Movex_Rnxxx(const TWord op)
	{
		const TWord ddddd	= getFieldValue<Movex_Rnxxx,Field_DDDD>(op);
		const TWord aaaaaaa	= getFieldValue<Movex_Rnxxx,Field_aaaaaa, Field_a>(op);
		const auto	write	= getFieldValue<Movex_Rnxxx,Field_W>(op);
		const TWord rrr		= getFieldValue<Movex_Rnxxx,Field_RRR>(op);

		const int shortDisplacement = signextend<int,7>(aaaaaaa);
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		const auto area = MemArea_X;

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
		const TWord aaaaaaa	= getFieldValue<Movey_Rnxxx,Field_aaaaaa, Field_a>(op);
		const auto	write	= getFieldValue<Movey_Rnxxx,Field_W>(op);
		const TWord rrr		= getFieldValue<Movey_Rnxxx,Field_RRR>(op);

		const int shortDisplacement = signextend<int,7>(aaaaaaa);
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		const auto area = MemArea_Y;

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
		const TWord mmmrrr	= getFieldValue<Movexr_ea,Field_MMM, Field_RRR>(op);
		const TWord ff		= getFieldValue<Movexr_ea,Field_ff>(op);
		const TWord write	= getFieldValue<Movexr_ea,Field_W>(op);
		const TWord d		= getFieldValue<Movexr_ea,Field_d>(op);

		// S2 D2 move
		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		if(F)		y1(ab);
		else		y0(ab);
		
		const TWord ea = decode_MMMRRR_read(mmmrrr);

		// S1/D1 move
		if( write )
		{
			if( mmmrrr == MMM_ImmediateData )
				decode_ee_write( ff, TReg24(ea) );
			else
				decode_ee_write( ff, TReg24(memRead(MemArea_X, ea)) );
		}
		else
		{
			const auto ea = decode_MMMRRR_read(mmmrrr);
			memWrite( MemArea_X, ea, decode_ff_read(ff).toWord());
		}
	}
	inline void DSP::op_Movexr_A(const TWord op)
	{
		const TWord mmmrrr	= getFieldValue<Movexr_A,Field_MMM, Field_RRR>(op);
		const TWord d		= getFieldValue<Movexr_A,Field_d>(op);

		const TWord ea = decode_MMMRRR_read(mmmrrr);

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
		const TWord mmmrrr	= getFieldValue<Moveyr_ea,Field_MMM, Field_RRR>(op);
		const TWord ff		= getFieldValue<Moveyr_ea,Field_ff>(op);
		const bool write	= getFieldValue<Moveyr_ea,Field_W>(op);
		const bool d		= getFieldValue<Moveyr_ea,Field_d>(op);

		// S2 D2 move
		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		if( e )		x1(ab);
		else		x0(ab);

		const TWord ea = decode_MMMRRR_read(mmmrrr);
		
		// S1/D1 move
		if( write )
		{
			if( mmmrrr == MMM_ImmediateData )
				decode_ff_write( ff, TReg24(ea) );
			else
				decode_ff_write( ff, TReg24(memRead(MemArea_Y, ea)) );
		}
		else
		{
			memWrite( MemArea_Y, ea, decode_ff_read( ff ).toWord() );
		}
	}
	inline void DSP::op_Moveyr_A(const TWord op)
	{
		const TWord mmmrrr	= getFieldValue<Movexr_A,Field_MMM, Field_RRR>(op);
		const TWord d		= getFieldValue<Movexr_A,Field_d>(op);
		const TWord ea 		= decode_MMMRRR_read(mmmrrr);

		const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
		memWrite( MemArea_Y, ea, ab.var );
		if (d)
			setB(y0());
		else
			setA(y0());
	}
	inline void DSP::op_Movel_ea(const TWord op)
	{
		const auto LLL		= getFieldValue<Movel_ea,Field_L, Field_LL>(op);
		const auto mmmrrr	= getFieldValue<Movel_ea,Field_MMM, Field_RRR>(op);
		const auto write	= getFieldValue<Movel_ea,Field_W>(op);

		const TWord ea = decode_MMMRRR_read(mmmrrr);

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
	inline void DSP::op_Movel_aa(const TWord op)
	{
		const auto LLL		= getFieldValue<Movel_aa,Field_L, Field_LL>(op);
		const auto write	= getFieldValue<Movel_aa,Field_W>(op);

		const TWord ea = getFieldValue<Movel_aa,Field_aaaaaa>(op);

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
	inline void DSP::op_Movexy(const TWord op)
	{
		const TWord mmrrr	= getFieldValue<Movexy,Field_MM, Field_RRR>(op);
		const TWord mmrr	= getFieldValue<Movexy,Field_mm, Field_rr>(op);
		const TWord writeX	= getFieldValue<Movexy,Field_W>(op);
		const TWord	writeY	= getFieldValue<Movexy,Field_w>(op);
		const TWord	ee		= getFieldValue<Movexy,Field_ee>(op);
		const TWord ff		= getFieldValue<Movexy,Field_ff>(op);

		// X
		const TWord eaX = decode_XMove_MMRRR( mmrrr );
		if( writeX )	decode_ee_write( ee, TReg24(memRead( MemArea_X, eaX )) );
		else			memWrite( MemArea_X, eaX, decode_ee_read( ee ).toWord() );

		// Y
		const TWord regIdxOffset = ((mmrrr&0x7) >= 4) ? 0 : 4;

		const TWord eaY = decode_YMove_mmrr( mmrr, regIdxOffset );
		if( writeY )	decode_ff_write( ff, TReg24(memRead( MemArea_Y, eaY )) );
		else			memWrite( MemArea_Y, eaY, decode_ff_read( ff ).toWord() );		
	}
	inline void DSP::op_Movec_ea(const TWord op)
	{
		const TWord ddddd	= getFieldValue<Movec_ea,Field_DDDDD>(op);
		const TWord mmmrrr	= getFieldValue<Movec_ea,Field_MMM, Field_RRR>(op);
		const auto write	= getFieldValue<Movec_ea,Field_W>(op);

		const TWord addr = decode_MMMRRR_read( mmmrrr );

		const EMemArea area = getFieldValueMemArea<Movec_ea>(op);
			
		if( write )
		{
			if( mmmrrr == MMM_ImmediateData )	decode_ddddd_pcr_write( ddddd, TReg24(addr) );		
			else								decode_ddddd_pcr_write( ddddd, TReg24(memRead( area, addr )) );
		}
		else
		{
			const TReg24 regVal = decode_ddddd_pcr_read(ddddd);
			assert( (mmmrrr != MMM_ImmediateData) && "register move to immediate data? not possible" );
			memWrite( area, addr, regVal.toWord() );
		}
	}
	inline void DSP::op_Movec_aa(const TWord op)
	{
		const TWord ddddd	= getFieldValue<Movec_aa,Field_DDDDD>(op);
		const TWord aaaaaa	= getFieldValue<Movec_aa,Field_aaaaaa>(op);
		const auto write	= getFieldValue<Movec_aa,Field_W>(op);

		const TWord addr = aaaaaa;

		const EMemArea area = getFieldValueMemArea<Movec_aa>(op);
			
		if( write )
		{
			decode_ddddd_pcr_write( ddddd, TReg24(memRead( area, addr )) );
		}
		else
		{
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
		const TWord mmmrrr	= getFieldValue<Movem_ea,Field_MMM, Field_RRR>(op);

		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		if( write )
		{
			assert( mmmrrr != MMM_ImmediateData && "immediate data should not be allowed here" );
			decode_dddddd_write( dddddd, TReg24(memRead( MemArea_P, ea )) );
		}
		else
		{
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

		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		if( write )
		{
			if( mmmrrr == MMM_ImmediateData )
				memWritePeriphFFFFC0( S, pp, ea );
			else
				memWritePeriphFFFFC0( S, pp, memRead( s, ea ) );
		}
		else
			memWrite( S, ea, memReadPeriphFFFFC0( s, pp ) );
	}
	inline void DSP::op_Movep_Xqqea(const TWord op)
	{
		const TWord mmmrrr	= getFieldValue<Movep_Xqqea,Field_MMM, Field_RRR>(op);
		const EMemArea S	= getFieldValueMemArea<Movep_Xqqea>(op);
		const TWord qAddr	= getFieldValue<Movep_Xqqea,Field_qqqqqq>(op);
		const auto write	= getFieldValue<Movep_Xqqea,Field_W>(op);

		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		const auto area = MemArea_X;

		if( write )
		{
			if( mmmrrr == MMM_ImmediateData )
				memWritePeriphFFFF80( area, qAddr, ea );
			else
				memWritePeriphFFFF80( area, qAddr, memRead( S, ea ) );
		}
		else
			memWrite( S, ea, memReadPeriphFFFF80( area, qAddr ) );				
	}
	inline void DSP::op_Movep_Yqqea(const TWord op)
	{
		// TODO: code dup op_Movep_Xqqea
		const TWord mmmrrr	= getFieldValue<Movep_Xqqea,Field_MMM, Field_RRR>(op);
		const EMemArea S	= getFieldValueMemArea<Movep_Xqqea>(op);
		const TWord qAddr	= getFieldValue<Movep_Xqqea,Field_qqqqqq>(op);
		const auto write	= getFieldValue<Movep_Xqqea,Field_W>(op);

		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		const auto area = MemArea_Y;

		if( write )
		{
			if( mmmrrr == MMM_ImmediateData )
				memWritePeriphFFFF80( area, qAddr, ea );
			else
				memWritePeriphFFFF80( area, qAddr, memRead( S, ea ) );
		}
		else
			memWrite( S, ea, memReadPeriphFFFF80( area, qAddr ) );				
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
	inline void DSP::op_Mpy_S1S2D(const TWord op)
	{
		exec_parallel_alu_multiply(op);
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

		const TReg24 s		= TReg24(fetchOpWordB());

		const TReg24 reg	= decode_qq_read(qq);

		alu_mpy( ab, reg, s, negate, false );
	}
	inline void DSP::op_Mpyr_S1S2D(const TWord op)
	{
		exec_parallel_alu_multiply(op);
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
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Nop(TWord)
	{
	}
	inline void DSP::op_Norm(const TWord op)
	{
		errNotImplemented("NORM");
	}
	inline void DSP::op_Normf(const TWord op)
	{
		errNotImplemented("NORMF");
	}
	inline void DSP::op_Not(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Or_SD(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
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
	inline void DSP::op_Pflush(const TWord op)
	{
		errNotImplemented("PFLUSH");		
	}
	inline void DSP::op_Pflushun(const TWord op)
	{
		cache.pflushun();		
	}
	inline void DSP::op_Pfree(const TWord op)
	{
		cache.pfree();		
	}
	inline void DSP::op_Plock(const TWord op)
	{
		cache.plock(fetchOpWordB());		
	}
	inline void DSP::op_Plockr(const TWord op)
	{
		errNotImplemented("PLOCKR");		
	}
	inline void DSP::op_Punlock(const TWord op)
	{
		errNotImplemented("PUNLOCK");
	}
	inline void DSP::op_Punlockr(const TWord op)
	{
		errNotImplemented("PUNLOCKR");		
	}
	inline void DSP::op_Rep_ea(const TWord op)
	{
		errNotImplemented("REP");
	}
	inline void DSP::op_Rep_aa(const TWord op)
	{
		errNotImplemented("REP");
	}
	inline void DSP::op_Rep_xxx(const TWord op)
	{
		const auto loopcount = getFieldValue<Rep_xxx,Field_hhhh, Field_iiiiiiii>(op);
		rep_exec(loopcount);
	}
	inline void DSP::op_Rep_S(const TWord op)
	{
		const int loopcount = decode_dddddd_read(getFieldValue<Rep_S,Field_dddddd>(op)).var;
		rep_exec(loopcount);
	}
	inline void DSP::op_Reset(const TWord op)
	{
		resetSW();
	}
	inline void DSP::op_Rnd(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Rol(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Ror(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);		
	}
	inline void DSP::op_Rti(const TWord op)
	{
		popPCSR();
		m_processingMode = DefaultPreventInterrupt;
	}
	inline void DSP::op_Rts(const TWord op)
	{
		popPC();		
	}
	inline void DSP::op_Sbc(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Stop(const TWord op)
	{
		errNotImplemented("STOP");
	}
	inline void DSP::op_Sub_SD(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
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
		convert( r56, TReg24(fetchOpWordB()) );

		alu_sub( ab, r56 );
	}
	inline void DSP::op_Subl(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_subr(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);		
	}
	inline void DSP::op_Tcc_S1D1(const TWord op)
	{
		const TWord cccc = getFieldValue<Tcc_S1D1,Field_CCCC>(op);

		if( decode_cccc( cccc ) )
		{
			const TWord JJJ = getFieldValue<Tcc_S1D1,Field_JJJ>(op);
			const bool ab = getFieldValue<Tcc_S1D1,Field_d>(op);

			decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
		}
	}
	inline void DSP::op_Tcc_S1D1S2D2(const TWord op)
	{
		const TWord cccc = getFieldValue<Tcc_S1D1S2D2,Field_CCCC>(op);

		if( decode_cccc( cccc ) )
		{
			const TWord TTT		= getFieldValue<Tcc_S1D1S2D2,Field_TTT>(op);
			const TWord JJJ		= getFieldValue<Tcc_S1D1S2D2,Field_JJJ>(op);
			const TWord ttt		= getFieldValue<Tcc_S1D1S2D2,Field_ttt>(op);
			const bool ab		= getFieldValue<Tcc_S1D1S2D2,Field_d>(op);

			decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
			reg.r[TTT] = reg.r[ttt];
		}
	}
	inline void DSP::op_Tcc_S2D2(const TWord op)
	{
		const TWord cccc = getFieldValue<Tcc_S2D2,Field_CCCC>(op);

		if( decode_cccc( cccc ) )
		{
			const TWord TTT		= getFieldValue<Tcc_S2D2,Field_TTT>(op);
			const TWord ttt		= getFieldValue<Tcc_S2D2,Field_ttt>(op);
			reg.r[TTT] = reg.r[ttt];
		}
	}
	inline void DSP::op_Tfr(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Trap(const TWord op)
	{
		errNotImplemented("TRAP");		
	}
	inline void DSP::op_Trapcc(const TWord op)
	{
		errNotImplemented("TRAPcc");		
	}
	inline void DSP::op_Tst(const TWord op)
	{
		exec_parallel_alu_nonMultiply(op);
	}
	inline void DSP::op_Vsl(const TWord op)
	{
		errNotImplemented("VSL");		
	}
	inline void DSP::op_Wait(const TWord op)
	{
		errNotImplemented("WAIT");		
	}
	inline void DSP::op_ResolveCache(const TWord op)
	{
		auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
		cacheEntry = Nop;
		
		--reg.ictr.var;
		--m_instructions;

		if( op )
		{
			if(Opcodes::isNonParallelOpcode(op))
			{
				const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);

				if(!oi)
				{
					m_opcodes.findNonParallelOpcodeInfo(op);		// retry here to help debugging
					assert(0 && "illegal instruction");
				}

				cacheEntry = oi->m_instruction;
			}
			else
			{
				const auto* oiMove = m_opcodes.findParallelMoveOpcodeInfo(op);
				if(!oiMove)
				{
					m_opcodes.findParallelMoveOpcodeInfo(op);		// retry here to help debugging
					assert(0 && "illegal instruction");
				}

				const OpcodeInfo* oiAlu = nullptr;

				if(op & 0xff)
				{
					oiAlu = m_opcodes.findParallelAluOpcodeInfo(op);
					if(!oiAlu)
					{
						m_opcodes.findParallelAluOpcodeInfo(op);	// retry here to help debugging
						assert(0 && "invalid instruction");						
					}
				}

				switch (oiMove->m_instruction)
				{
				case Move_Nop:
					// Only ALU, no parallel move
					cacheEntry = oiAlu ? oiAlu->m_instruction : Nop;
					break;
				case Ifcc:
				case Ifcc_U:
					// IFcc executes the ALU instruction if the condition is met, therefore no ALU exec by us
					cacheEntry = oiAlu ? oiMove->m_instruction : Nop;
					break;
				default:
					if(!oiAlu)
					{
						// if there is no ALU instruction, do only a move
						cacheEntry = oiMove->m_instruction;
					}
					else
					{
						// call special function that simulates latch registers for alu op + parallel move
						cacheEntry = Parallel | oiMove->m_instruction << 8 | oiAlu->m_instruction << 16;
					}
				}
			}

			execOp(op);
		}
		else
		{
			execOp(0);
		}
	}
	inline void DSP::op_Parallel(const TWord op)
	{
		const auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
		const auto instMove = static_cast<Instruction>((cacheEntry >> 8) & 0xff);
		const auto instAlu = static_cast<Instruction>((cacheEntry >> 16) & 0xff);

		exec_parallel(instMove, instAlu, op);
	}
}
