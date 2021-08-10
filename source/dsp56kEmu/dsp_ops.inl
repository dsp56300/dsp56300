#pragma once

#include "dsp_jumptable.inl"
#include "types.h"

namespace dsp56k
{
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
		const TWord bit		= getBit<Bchg_D>(op);
		const TWord dddddd	= getFieldValue<Bchg_D,Field_DDDDDD>(op);

		TReg24 val = decode_dddddd_read( dddddd );

		sr_toggle( CCR_C, bittestandchange( val, bit ) );

		decode_dddddd_write( dddddd, val );
	}
	inline void DSP::op_Bclr_ea(const TWord op)	// 0000101001MMMRRR0S0bbbbb
	{
		const EMemArea S = getFieldValueMemArea<Bclr_ea>(op);

		const TWord bbbbb = getBit<Bclr_qq>(op);

		const TWord ea = effectiveAddress<Bclr_ea>(op);
		
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
		const TWord bit = getBit<Bclr_pp>(op);
		const TWord ea	= getFieldValue<Bclr_pp,Field_pppppp>(op);

		const EMemArea S = getFieldValueMemArea<Bclr_pp>(op);

		const TWord res = alu_bclr( bit, memReadPeriphFFFFC0( S, ea ) );

		memWritePeriphFFFFC0( S, ea, res );			
	}
	inline void DSP::op_Bclr_qq(const TWord op)
	{
		const TWord bit = getBit<Bclr_qq>(op);
		const TWord ea	= getFieldValue<Bclr_qq,Field_qqqqqq>(op);

		const EMemArea S = getFieldValueMemArea<Bclr_qq>(op);

		const TWord res = alu_bclr( bit, memReadPeriphFFFF80( S, ea ) );

		memWritePeriphFFFF80( S, ea, res );			
	}
	inline void DSP::op_Bclr_D(const TWord op)
	{
		const TWord bit		= getBit<Bclr_D>(op);
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
		const TWord bit		= getBit<Bset_ea>(op);
		const EMemArea S	= getFieldValueMemArea<Bset_ea>(op);
		const TWord ea		= effectiveAddress<Bset_ea>(op);

		if(ea >= XIO_Reserved_High_First)
		{
			// god WHY is this even possible! Bset_pp/qq are for peripherals and even save one word!	TODO: code optimizer? We could rewrite as Bset_qq/pp + one nop
			auto val = memReadPeriph( S, ea );

			sr_toggle( CCR_C, bittestandset( val, bit ) );

			memWritePeriph( S, ea, val );
		}
		else
		{
			auto val = memRead( S, ea );

			sr_toggle( CCR_C, bittestandset( val, bit ) );

			memWrite( S, ea, val );
		}
	}
	inline void DSP::op_Bset_aa(const TWord op)
	{
		errNotImplemented("BSET");
	}
	inline void DSP::op_Bset_pp(const TWord op)	// 0000101010pppppp0S1bbbbb
	{
		const TWord bit		= getBit<Bset_pp>(op);
		const TWord pppppp	= getFieldValue<Bset_pp,Field_pppppp>(op);
		const EMemArea S	= getFieldValueMemArea<Bset_pp>(op);

		const TWord ea		= pppppp;

		TWord val = memReadPeriphFFFFC0( S, ea );

		sr_toggle( CCR_C, bittestandset( val, bit ) );

		memWritePeriphFFFFC0( S, ea, val );
	}
	inline void DSP::op_Bset_qq(const TWord op)
	{
		const TWord bit		= getBit<Bset_qq>(op);
		const TWord qqqqqq	= getFieldValue<Bset_qq,Field_qqqqqq>(op);
		const EMemArea S	= getFieldValueMemArea<Bset_qq>(op);

		const TWord ea		= qqqqqq;

		TWord val = memReadPeriphFFFF80( S, ea );

		sr_toggle( CCR_C, bittestandset( val, bit ) );

		memWritePeriphFFFF80( S, ea, val );
	}
	inline void DSP::op_Bset_D(const TWord op)
	{
		const TWord bit	= getBit<Bset_D>(op);
		const TWord d	= getFieldValue<Bset_D,Field_DDDDDD>(op);

		TReg24 val = decode_dddddd_read(d);

		if( (d & 0x3f) == 0x39 )	// is SR the destination?	TODO: magic value
		{
			bittestandset( val.var, bit );
		}
		else
		{
			sr_toggle( CCR_C, bittestandset( val, bit ) );
		}

		decode_dddddd_write( d, val );
	}

	inline void DSP::op_Btst_ea(const TWord op)
	{
		sr_toggle(CCR_C, bitTestMemory<Btst_ea>(op));
	}
	inline void DSP::op_Btst_aa(const TWord op)
	{
		sr_toggle(CCR_C, bitTestMemory<Btst_aa>(op));
	}
	inline void DSP::op_Btst_pp(const TWord op)
	{
		const TWord bitNum	= getBit<Btst_pp>(op);
		const TWord pppppp	= getFieldValue<Btst_pp,Field_pppppp>(op);
		const EMemArea S	= getFieldValueMemArea<Btst_pp>(op);

		const TWord memVal	= memReadPeriphFFFFC0( S, pppppp );

		const bool bitSet	= ( memVal & (1<<bitNum)) != 0;

		sr_toggle( CCR_C, bitSet );
	}
	inline void DSP::op_Btst_qq(const TWord op)
	{
		errNotImplemented("BTST qq");
	}
	inline void DSP::op_Btst_D(const TWord op)
	{
		const TWord dddddd	= getFieldValue<Btst_D,Field_DDDDDD>(op);
		const TWord bit		= getBit<Btst_D>(op);

		TReg24 val = decode_dddddd_read( dddddd );

		sr_toggle( CCR_C, bittest( val.var, bit ) );
	}
	inline void DSP::op_Debug(const TWord op)
	{
		LOG( "Entering DEBUG mode" );
		errNotImplemented("DEBUG");		
	}
	inline void DSP::op_Debugcc(const TWord op)
	{
		if( checkCondition<Debugcc>(op) )
		{
			LOG( "Entering DEBUG mode because condition is met" );
			errNotImplemented("DEBUGcc");
		}
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
		const TWord addr = absAddressExt<Do_xxx>();
		const TWord loopcount = getFieldValue<Do_xxx,Field_hhhh, Field_iiiiiiii>(op);

		do_exec( loopcount, addr );
	}
	inline void DSP::op_Do_S(const TWord op)
	{
		const TWord addr = absAddressExt<Do_S>();

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
        const auto displacement = pcRelativeAddressExt<Dor_xxx>();
        do_exec(loopcount, pcCurrentInstruction + displacement);
	}
	inline void DSP::op_Dor_S(const TWord op)
	{
		const TWord dddddd = getFieldValue<Dor_S,Field_DDDDDD>(op);
		const TReg24 lc		= decode_dddddd_read( dddddd );
		
		const int displacement = pcRelativeAddressExt<Dor_S>();
		do_exec( lc.var, pcCurrentInstruction + displacement);
	}
	inline void DSP::op_DorForever(const TWord op)
	{
		errNotImplemented("DOR FOREVER");		
	}
	inline void DSP::op_Enddo(const TWord op)
	{
		do_end();
	}
	inline void DSP::op_Ifcc(const TWord op)
	{
		if( checkCondition<Ifcc>(op) )
		{
			const auto backupCCR = ccr();

			const auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
			const auto& instAlu = cacheEntry.opAlu;

			exec_jump(instAlu, op);

			ccr(backupCCR);
		}
	}
	inline void DSP::op_Ifcc_U(const TWord op)
	{
		if( checkCondition<Ifcc_U>(op) )
		{
			const auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
			const auto& instAlu = cacheEntry.opAlu;

			exec_jump(instAlu, op);
		}
	}
	inline void DSP::op_Illegal(const TWord op)
	{
		errNotImplemented("ILLEGAL");
	}

	inline void DSP::op_Lra_Rn(const TWord op)
	{
		errNotImplemented("LRA");
	}
	inline void DSP::op_Lra_xxxx(const TWord op)	// 0000010001oooooo010ddddd
	{
		const TWord ddddd = getFieldValue<Lra_xxxx, Field_ddddd>(op);
		const auto ea = pcCurrentInstruction + pcRelativeAddressExt<Lra_xxxx>();
		decode_ddddd_write(ddddd, TReg24(ea));
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
		case 0x00:	/* 00 */	AGU::updateAddressRegister( _r, -_n.var, _m.var,moduloMask[regIdx],modulo[regIdx] );		break;
		case 0x08:	/* 01 */	AGU::updateAddressRegister( _r, +_n.var, _m.var,moduloMask[regIdx],modulo[regIdx] );		break;
		case 0x10:	/* 10 */	AGU::updateAddressRegister( _r, -1, _m.var,moduloMask[regIdx],modulo[regIdx] );			break;
		case 0x18:	/* 11 */	AGU::updateAddressRegister( _r, +1, _m.var,moduloMask[regIdx],modulo[regIdx] );			break;
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

		// TODO: modulo not taken into account, but it IS USED, it tested this in the simulator
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
	void DSP::cachePlock(const TWord _effectiveAddress)
	{
		cache.plock(_effectiveAddress);
	}
	inline void DSP::op_Plock(const TWord op)
	{
		cachePlock(effectiveAddress<Plock>(op));
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
	inline void DSP::op_Rti(const TWord op)
	{
		popPCSR();
		m_processingMode = DefaultPreventInterrupt;
		m_interruptFunc = &DSP::execDefaultPreventInterrupt;
	}
	inline void DSP::op_Rts(const TWord op)
	{
		popPC();
	}
	inline void DSP::op_Stop(const TWord op)
	{
		errNotImplemented("STOP");
	}
	inline void DSP::op_Tcc_S1D1(const TWord op)
	{
		if( checkCondition<Tcc_S1D1>(op) )
		{
			const TWord JJJ = getFieldValue<Tcc_S1D1,Field_JJJ>(op);
			const bool ab = getFieldValue<Tcc_S1D1,Field_d>(op);

			decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
		}
	}
	inline void DSP::op_Tcc_S1D1S2D2(const TWord op)
	{
		if( checkCondition<Tcc_S1D1S2D2>(op) )
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
		if( checkCondition<Tcc_S2D2>(op) )
		{
			const TWord TTT		= getFieldValue<Tcc_S2D2,Field_TTT>(op);
			const TWord ttt		= getFieldValue<Tcc_S2D2,Field_ttt>(op);
			reg.r[TTT] = reg.r[ttt];
		}
	}
	inline void DSP::op_Trap(const TWord op)
	{
		errNotImplemented("TRAP");		
	}
	inline void DSP::op_Trapcc(const TWord op)
	{
		if(checkCondition<Trapcc>(op))
			errNotImplemented("TRAPcc");
	}
	inline void DSP::op_Wait(const TWord op)
	{
		errNotImplemented("WAIT");		
	}
	inline void DSP::op_ResolveCache(const TWord op)
	{
		auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
		cacheEntry.op = &DSP::op_Nop;

		if( !op )
		{
			op_Nop(0);
			return;
		}

		if(Opcodes::isNonParallelOpcode(op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);

			if(!oi)
			{
				m_opcodes.findNonParallelOpcodeInfo(op);		// retry here to help debugging
				assert(0 && "illegal instruction");
			}

			cacheEntry.op = resolvePermutation(oi->m_instruction, op);

			exec_jump(cacheEntry.op, op);
			return;
		}
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
			if(oiAlu)
			{
				cacheEntry.op = resolvePermutation(oiAlu->m_instruction, op);
				exec_jump(cacheEntry.op, op);
			}
			else
			{
				op_Nop(op);
			}
			break;
		case Ifcc:
		case Ifcc_U:
			// IFcc executes the ALU instruction if the condition is met, therefore no ALU exec by us
			if(oiAlu)
			{
				const auto ifccFunc = resolvePermutation(oiMove->m_instruction, op);
				cacheEntry.op = ifccFunc;
				cacheEntry.opAlu = resolvePermutation(oiAlu->m_instruction, op);
				exec_jump(cacheEntry.op, op);
			}
			else
			{
				op_Nop(op);						
			}
			break;
		default:
			if(!oiAlu)
			{
				// if there is no ALU instruction, do only the move
				cacheEntry.op = resolvePermutation(oiMove->m_instruction, op);
				exec_jump(cacheEntry.op, op);
			}
			else
			{
				// call special function that simulates latch registers for alu op + parallel move
				cacheEntry.op = &DSP::op_Parallel;
				cacheEntry.opMove = resolvePermutation(oiMove->m_instruction, op);
				cacheEntry.opAlu = resolvePermutation(oiAlu->m_instruction, op);
				op_Parallel(op);
			}
		}
	}

	inline void DSP::op_Parallel(const TWord op)
	{
		const auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
		const auto& instMove = cacheEntry.opMove;
		const auto& instAlu = cacheEntry.opAlu;

		exec_parallel(instMove, instAlu, op);
	}
}
