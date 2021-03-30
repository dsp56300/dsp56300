// DSP 56300 family 24-bit DSP emulator

#include "pch.h"

#include "dsp.h"
#include "registers.h"
#include "types.h"
#include "memory.h"
#include "error.h"
#include "agu.h"
#include "disasm.h"
#include "opcodes.h"

#if 0
#	define LOGSC(F)	logSC(F)
#else
#	define LOGSC(F)	{}
#endif

//

#ifdef _MSC_VER
#pragma warning(disable:4355)	// warning C4355: 'this' : used in base member initializer list
#endif

namespace dsp56k
{
	static bool g_dumpPC = true;
//	static const TWord g_dumpPCictrMin = 0x153000;
	static const TWord g_dumpPCictrMin = 0xc0000;

	// _____________________________________________________________________________
	// DSP
	//
	DSP::DSP( Memory& _memory ) : mem(_memory)
		, pcLastExec(0xffffff)
		, repRunning(false)
		, essi(*this,_memory)
	{
		mem.setDSP(this);

		m_asm[0] = 0;

		resetHW();
	}

	// _____________________________________________________________________________
	// resetHW
	//
	void DSP::resetHW()
	{
		// 100162AEd01.pdf - page 2-16

		// TODO: internal peripheral devices are reset

		reg.m[0] = reg.m[1] = reg.m[2] = reg.m[3] = reg.m[4] = reg.m[5] = reg.m[6] = reg.m[7] = TReg24(int(0xffffff));
		reg.r[0] = reg.r[1] = reg.r[2] = reg.r[3] = reg.r[4] = reg.r[5] = reg.r[6] = reg.r[7] = TReg24(int(0));
		reg.n[0] = reg.n[1] = reg.n[2] = reg.n[3] = reg.n[4] = reg.n[5] = reg.n[6] = reg.n[7] = TReg24(int(0));

		reg.iprc = reg.iprp = TReg24(int(0));

		// TODO: The Bus Control Register (BCR), the Address Attribute Registers (AAR3–AAR0) and the DRAM Control Register (DCR) are set to their initial values as described in Chapter 9, External Memory Interface (Port A). The initial value causes a maximum number of wait states to be added to every external memory access.

		reg.sp = TReg24(int(0));
		reg.sc = TReg5(char(0));

		reg.sz.var = 0xbadbad; // The SZ register is not initialized during hardware reset, and must be set, using a MOVEC instruction, prior to enabling the stack extension.

		const TWord srClear	= SR_RM | SR_SM | SR_CE | SR_SA | SR_FV | SR_LF | SR_DM | SR_SC | SR_S0 | SR_S1 | 0xf;
		const TWord srSet		= SR_CP0 | SR_CP1 | SR_I0 | SR_I1;

		sr_clear( srClear );
		sr_set	( srSet );

		// The Instruction Cache Controller is initialized as described in Chapter 8,	Instruction Cache.
		cache.reset();
		sr_clear( SR_CE );

		// TODO: The PLL Control register is initialized as described in Chapter 6, PLL and Clock Generator.

		reg.vba = TReg24(int(0));

		// TODO: The DSP56300 core remains in the Reset state until RESET is deasserted. Upon leaving the Reset state, the Chip Operating mode bits of the OMR are loaded from the external mode select pins (MODA, MODB, MODC, MODD), and program execution begins at the program memory address as described in Chapter 11, Operating Modes and Memory Spaces.

		reg.pc = TReg24(int(0));
		reg.omr = TReg24(int(0));

		reg.ictr.var = 0;
	}

	// _____________________________________________________________________________
	// exec
	//
	void DSP::exec()
	{
		// we do not support 16-bit compatibility mode
		assert( (reg.sr.var & SR_SC) == 0 && "16 bit compatibility mode is not supported");

		essi.exec();

#ifdef _DEBUG
		getASM();

		if( g_dumpPC && pcLastExec != reg.pc.toWord() && reg.ictr.var >= g_dumpPCictrMin )
		{
			std::stringstream ssout;

			for( int i=0; i<reg.sc.var; ++i )
				ssout << "\t";

			ssout << "EXEC @ " << std::hex << reg.pc.toWord() << " asm = " << m_asm;

			LOGF( ssout.str() );
		}

#else
		m_asm[0] = 0;
#endif

		pcLastExec = reg.pc.toWord();

		// next instruction word
		const TWord op = fetchPC();

		if( !op )
		{
#ifdef _DEBUG
//			LOG( "nop" );
#endif
		}
		// non-parallel instruction
		else if(Opcodes::isNonParallelOpcode(op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);
			if(!oi)
			{
				m_opcodes.findNonParallelOpcodeInfo(op);	// retry here to help debugging
				assert(0 && "illegal instruction");
			}

			if(!exec_nonParallel(oi, op))
				assert( 0 && "illegal instruction" );
		}
		else if( !exec_parallel(op))
		{
			exec_parallel(op);	// retry to debug
			assert( 0 && "illegal instruction" );
		}

		if( reg.ictr.var >= g_dumpPCictrMin )
		{
			for( size_t i=0; i<Reg_COUNT; ++i )
			{
				if( i == Reg_PC || i == Reg_ICTR )
					continue;

				int64_t regVal = 0;
				const bool r = readRegToInt( (EReg)i, regVal );

				if( !r )
					continue;
	//			assert( r && "failed to read register" );

				if( regVal != m_prevRegStates[i].val )
				{
					SRegChange regChange;
					regChange.reg = (EReg)i;
					regChange.valOld.var = (int)m_prevRegStates[i].val;
					regChange.valNew.var = (int)regVal;
					regChange.pc = pcLastExec;
					regChange.ictr = reg.ictr.var;

					m_regChanges.push_back( regChange );

					LOGF( "Reg " << g_regNames[i] << " changed @ " << std::hex << regChange.pc << ", ictr " << regChange.ictr << ", old " << std::hex << regChange.valOld.var << ", new " << regChange.valNew.var );

					m_prevRegStates[i].val = regVal;
				}
			}
		}

		++reg.ictr.var;
	}

	bool DSP::exec_parallel(const TWord op)
	{
		const auto* oi = m_opcodes.findParallelMoveOpcodeInfo(op & 0xffff00);

		switch (oi->getInstruction())
		{
		case OpcodeInfo::Ifcc:		// execute the specified ALU operation of the condition is true
		case OpcodeInfo::Ifcc_U:
			{
				const auto cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);
				if( !decode_cccc( cccc ) )
					return true;

				const auto backupCCR = ccr();
				const auto res = exec_parallel_alu(op);
				if(oi->getInstruction() == OpcodeInfo::Ifcc)
					ccr(backupCCR);
				return res;
			}
		case OpcodeInfo::Move_Nop:
			return exec_parallel_alu(op);
		default:
			// simulate latches registers for parallel instructions
			const auto preMoveX = reg.x;
			const auto preMoveY = reg.y;
			const auto preMoveA = reg.a;
			const auto preMoveB = reg.b;

			auto res = exec_parallel_move(oi, op);

			const auto postMoveX = reg.x;
			const auto postMoveY = reg.y;
			const auto postMoveA = reg.a;
			const auto postMoveB = reg.b;

			// restore previous state for the ALU to process them
			reg.x = preMoveX;
			reg.y = preMoveY;
			reg.a = preMoveA;
			reg.b = preMoveB;

			res |= exec_parallel_alu(op);

			// now check what has changed and get the final values for all registers
			if( postMoveX != preMoveX )
			{
				assert( preMoveX == reg.x && "ALU changed a register at the same time the MOVE command changed it!" );
				reg.x = postMoveX;
			}
			else if( reg.x != preMoveX )
			{
				assert( preMoveX == postMoveX && "ALU changed a register at the same time the MOVE command changed it!" );
			}

			if( postMoveY != preMoveY )
			{
				assert( preMoveY == reg.y && "ALU changed a register at the same time the MOVE command changed it!" );
				reg.y = postMoveY;
			}
			else if( reg.y != preMoveY )
			{
				assert( preMoveY == postMoveY && "ALU changed a register at the same time the MOVE command changed it!" );
			}

			if( postMoveA != preMoveA )
			{
				assert( preMoveA == reg.a && "ALU changed a register at the same time the MOVE command changed it!" );
				reg.a = postMoveA;
			}
			else if( reg.a != preMoveA )
			{
				assert( preMoveA == postMoveA && "ALU changed a register at the same time the MOVE command changed it!" );
			}

			if( postMoveB != preMoveB )
			{
				assert( preMoveB == reg.b && "ALU changed a register at the same time the MOVE command changed it!" );
				reg.b = postMoveB;
			}
			else if( reg.b != preMoveB )
			{
				assert( preMoveB == postMoveB && "ALU changed a register at the same time the MOVE command changed it!" );
			}

			return res;
		}
	}

	bool DSP::exec_parallel_alu(TWord op)
	{
		const auto* opInfo = m_opcodes.findParallelAluOpcodeInfo(op);

		op &= 0xff;

		switch(op>>7)
		{
		case 0:
			return exec_parallel_alu_nonMultiply(op);
		case 1:
			return exec_parallel_alu_multiply(op);
		default:
			assert(0 && "invalid op");
			return false;
		}
	}

	bool DSP::exec_parallel_alu_nonMultiply(TWord op)
	{
		const auto kkk = op & 0x7;
		const auto D = (op>>3) & 0x1;
		const auto JJJ = (op>>4) & 0x7;

		if(!JJJ && !kkk)
			return true;	// TODO: docs say "MOVE" does it mean no ALU op?

		if(!kkk)
		{
			alu_add(D, decode_JJJ_read_56(JJJ, !D));
			return true;
		}

		if(kkk == 4)
		{
			alu_sub(D, decode_JJJ_read_56(JJJ, !D));
			return true;
		}

		if(JJJ >= 4)
		{
			switch (kkk)
			{
			case 1:		alu_tfr(D, decode_JJJ_read_56(JJJ, !D));			return true;
			case 2:		alu_or(D, decode_JJJ_read_24(JJJ, !D).var);			return true;
			case 3:		LOG_ERR_NOTIMPLEMENTED("eor");						return true;	// alu_eor(D, decode_JJJ_read_24(JJJ, !D).var);
			case 5:		alu_cmp(D, decode_JJJ_read_56(JJJ, !D), false);		return true;
			case 6:		alu_and(D, decode_JJJ_read_24(JJJ, !D).var);		return true;
			case 7:		alu_cmp(D, decode_JJJ_read_56(JJJ, !D), true);		return true;
			}
		}

		switch (JJJ)
		{
		case 0:
			switch (kkk)
			{
			case 1:		alu_tfr(D, decode_JJJ_read_56(JJJ, !D));			return true;
			case 2:		alu_addr(D);										return true;
			case 3:		alu_tst(D);											return true;
			case 5:		alu_cmp(D, D ? reg.a : reg.b, false);				return true;
			case 6:		LOG_ERR_NOTIMPLEMENTED("addr");						return true;	// alu_subr
			case 7:		alu_cmp(D, D ? reg.a : reg.b, true);				return true;
			}
		case 1:
			switch (kkk)
			{
			case 1:		alu_rnd(D);								return true;
			case 2:		alu_addl(D);							return true;
			case 3:		alu_clr(D);								return true;
			case 6:		LOG_ERR_NOTIMPLEMENTED("subl");			return true;	// alu_subl
			case 7:		LOG_ERR_NOTIMPLEMENTED("not");			return true;	// alu_not
			}
			break;
		case 2:
			switch (kkk)
			{
			case 2:		alu_asr(D, !D, 1);						return true;
			case 3:		alu_lsr(D, 1);							return true;
			case 6:		alu_abs(D);								return true;
			case 7:		LOG_ERR_NOTIMPLEMENTED("ror");			return true;	// alu_ror
			}
			break;
		case 3:
			switch (kkk)
			{
			case 2:		alu_asl(D, !D, 1);						return true;
			case 3:		alu_lsl(D, 1);							return true;
			case 6:		alu_neg(D);								return true;
			case 7:		LOG_ERR_NOTIMPLEMENTED("rol");			return true;	// alu_rol
			}
			break;
		case 4:
		case 5:
			switch (kkk)
			{
			case 1:		LOG_ERR_NOTIMPLEMENTED("adc");			return true;	// alu_adc
			case 5:		LOG_ERR_NOTIMPLEMENTED("sbc");			return true;	// alu_sbc
			}
		}
		return false;
	}

	bool DSP::exec_parallel_alu_multiply(const TWord op)
	{
		const auto round = op & 0x1;
		const auto mulAcc = (op>>1) & 0x1;
		const auto negative = (op>>2) & 0x1;
		const auto ab = (op>>3) & 0x1;
		const auto qqq = (op>>4) & 0x7;

		TReg24 s1, s2;

		decode_QQQ_read(s1, s2, qqq);

		// TODO: ccr
		alu_mpy(ab, s1, s2, negative, mulAcc);
		if(round)
		{
			alu_rnd(ab);
		}

		return true;
	}

	// _____________________________________________________________________________
	// exec_parallel_move
	//
	bool DSP::exec_parallel_move(const OpcodeInfo* oi, const TWord op)
	{
		switch (oi->m_instruction)
		{
		case OpcodeInfo::Move_xx:		// (...) #xx,D - Immediate Short Data Move - 001dddddiiiiiiii
			{
				const TWord ddddd = oi->getFieldValue(OpcodeInfo::Field_ddddd, op);
				const TReg8 iiiiiiii = TReg8(static_cast<uint8_t>(oi->getFieldValue(OpcodeInfo::Field_iiiiiiii, op)));

				decode_ddddd_write(ddddd, iiiiiiii);
			}
			return true;
		case OpcodeInfo::Mover:
			{
				const auto eeeee = oi->getFieldValue(OpcodeInfo::Field_eeeee, op);
				const auto ddddd = oi->getFieldValue(OpcodeInfo::Field_ddddd, op);

				// TODO: we need to determine the two register types and call different template versions of read and write
				decode_ddddd_write<TReg24>( ddddd, decode_ddddd_read<TReg24>( eeeee ) );
			}
			return true;
		case OpcodeInfo::Move_ea:						// does not move but updates r[x]
			{
				const TWord mmrrr = oi->getFieldValue(OpcodeInfo::Field_MM, OpcodeInfo::Field_RRR, op);
				decode_MMMRRR_read( mmrrr );
			}
			return true;
		case OpcodeInfo::Movex_ea:		// (...) X:ea<>D - 01dd0ddd W1MMMRRR			X Memory Data Move
		case OpcodeInfo::Movey_ea:		// (...) Y:ea<>D - 01dd1ddd W1MMMRRR			Y Memory Data Move
			{
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const TWord ddddd	= oi->getFieldValue(OpcodeInfo::Field_dd, OpcodeInfo::Field_ddd, op);
				const TWord write	= oi->getFieldValue(OpcodeInfo::Field_W, op);

				exec_move_ddddd_MMMRRR( ddddd, mmmrrr, write, oi->getInstruction() == OpcodeInfo::Movex_ea ? MemArea_X : MemArea_Y );
			}
			return true;
		case OpcodeInfo::Movex_aa: 
			LOG_ERR_NOTIMPLEMENTED("Movex_aa");
			return true;
		case OpcodeInfo::Movexr_ea:		// X Memory and Register Data Move		(...) X:ea,D1 S2,D2 - 0001ffdF W0MMMRRR
			{
				const TWord F		= oi->getFieldValue(OpcodeInfo::Field_F, op);	// true:Y1, false:Y0
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const TWord ff		= oi->getFieldValue(OpcodeInfo::Field_ff, op);
				const TWord write	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const TWord d		= oi->getFieldValue(OpcodeInfo::Field_d, op);

				// S2 D2 move
				const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
				if(F)		y1(ab);
				else		y0(ab);

				// S1/D1 move
				if( write )
				{
					decode_ff_write( ff, TReg24(decode_MMMRRR_read(mmmrrr)) );
				}
				else
				{
					const auto ea = decode_MMMRRR_read(mmmrrr);
					memWrite( MemArea_X, ea, decode_ff_read(ff).toWord());
				}
			}
			return true;
		case OpcodeInfo::Movexr_A: 
			LOG_ERR_NOTIMPLEMENTED("Movexr_A");
			return true;
		case OpcodeInfo::Movey_aa: 
			LOG_ERR_NOTIMPLEMENTED("Movey_aa");
			return true;
		case OpcodeInfo::Moveyr_ea:			// Register and Y Memory Data Move - (...) S1,D1 Y:ea,D2 - 0001deff W1MMMRRR
			{
				const bool e		= oi->getFieldValue(OpcodeInfo::Field_e, op);	// true:X1, false:X0
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const TWord ff		= oi->getFieldValue(OpcodeInfo::Field_ff, op);
				const bool write	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const bool d		= oi->getFieldValue(OpcodeInfo::Field_d, op);

				// S2 D2 move
				const TReg24 ab = d ? getB<TReg24>() : getA<TReg24>();
				if( e )		x1(ab);
				else		x0(ab);

				// S1/D1 move

				if( write )
				{
					if( mmmrrr == MMM_ImmediateData )
						decode_ff_write( ff, TReg24(fetchPC()) );
					else
						decode_ff_write( ff, TReg24(decode_MMMRRR_read(mmmrrr)) );
				}
				else
				{
					const TWord ea = decode_MMMRRR_read(mmmrrr);
					memWrite( MemArea_Y, ea, decode_ff_read( ff ).toWord() );
				}
			}
			return true;
		case OpcodeInfo::Moveyr_A:
			// TODO: take care on ordering for instructions such as "move #>$4cd49,a a,y0", is a moved to y first and THEN overwritten?
			LOG_ERR_NOTIMPLEMENTED("MOVE YR A");
			return true;
		case OpcodeInfo::Movel_ea:				// Long Memory Data Move - 0100L0LLW1MMMRRR
			{
				const auto LLL		= oi->getFieldValue(OpcodeInfo::Field_L, OpcodeInfo::Field_LL, op);
				const auto mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const auto write	= oi->getFieldValue(OpcodeInfo::Field_W, op);

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
			return true;
		case OpcodeInfo::Movel_aa: 
			LOG_ERR_NOTIMPLEMENTED("MOVE L aa");
			return true;
		case OpcodeInfo::Movexy:					// XY Memory Data Move - 1wmmeeff WrrMMRRR
			{
				const TWord mmrrr	= oi->getFieldValue(OpcodeInfo::Field_MM, OpcodeInfo::Field_RRR, op);
				const TWord mmrr	= oi->getFieldValue(OpcodeInfo::Field_mm, OpcodeInfo::Field_rr, op);
				const TWord writeX	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const TWord	writeY	= oi->getFieldValue(OpcodeInfo::Field_w, op);
				const TWord	ee		= oi->getFieldValue(OpcodeInfo::Field_ee, op);
				const TWord ff		= oi->getFieldValue(OpcodeInfo::Field_ff, op);

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
			return true;
		default:
			return false;
		}
	}
	
	inline bool DSP::exec_nonParallel(const OpcodeInfo* oi, TWord op)
	{
		if( exec_operand_8bits(oi, op&0xff) )	return true;
		if( exec_move(oi, op) )					return true;
		if( exec_pcu(oi, op) )					return true;
		if( exec_bitmanip(oi, op) )				return true;
		if( exec_logical_nonparallel(oi, op) )	return true;
		
		switch (oi->getInstruction())
		{
		case OpcodeInfo::Add_xx:	// 0000000101iiiiii10ood000
			{
				const TWord iiiiii = oi->getFieldValue(OpcodeInfo::Field_iiiiii, op);
				const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);

				alu_add( ab, TReg56(iiiiii) );
			}
			return true;
		case OpcodeInfo::Add_xxxx:
			{
				const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);

				TReg56 r56;
				convert( r56, TReg24(fetchPC()) );

				alu_add( ab, r56 );
			}
			return true;
		// Subtract
		case OpcodeInfo::Sub_xx:
			{
				const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);
				const TWord iiiiii = oi->getFieldValue(OpcodeInfo::Field_iiiiii, op);

				alu_sub( ab, TReg56(iiiiii) );
			}
			return true;
		case OpcodeInfo::Sub_xxxx:	// 0000000101ooooo011ood100
			{
				const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);

				TReg56 r56;
				convert( r56, TReg24(fetchPC()) );

				alu_sub( ab, r56 );
			}
			return true;
		case OpcodeInfo::BRKcc:					// Exit Current DO Loop Conditionally
			LOG_ERR_NOTIMPLEMENTED("BRKcc");
			return true;
		case OpcodeInfo::Clb:					// Count Leading Bits - // CLB S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 S D
			LOG_ERR_NOTIMPLEMENTED("CLB");
			return true;
		// Compare
		case OpcodeInfo::Cmp_xxS2:				// CMP #xx, S2 - 00000001 01iiiiii 1000d101
			{
				const TWord iiiiii = oi->getFieldValue(OpcodeInfo::Field_iiiiii, op);
				
				TReg56 r56;
				convert( r56, TReg24(iiiiii) );

				alu_cmp( bittest(op,3), r56, false );				
			}
			return true;
		case OpcodeInfo::Cmp_xxxxS2:
			{
				const TReg24 s( signextend<int,24>( fetchPC() ) );

				TReg56 r56;
				convert( r56, s );

				alu_cmp( bittest(op,3), r56, false );				
			}
			return true;
		// Compare Unsigned
		case OpcodeInfo::Cmpu_S1S2:
			LOG_ERR_NOTIMPLEMENTED("CMPU");
			return true;
		case OpcodeInfo::Div:	// 0000000110oooooo01JJdooo
			{
				// TODO: i'm not sure if this works as expected...

				const TWord jj = oi->getFieldValue(OpcodeInfo::Field_JJ, op);
				const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);

				TReg56& d = ab ? reg.b : reg.a;

				TReg24 s24 = decode_JJ_read( jj );

				const TReg56 debugOldD = d;
				const TReg24 debugOldS = s24;

				bool c = bittest(d,55) != bittest(s24,23);

				bool old47 = bittest(d,47);

				d.var <<= 1;

				bool changed47 = bittest( d, 47 ) != c;

				if( sr_test(SR_C) )
					bittestandset( d.var, 0 );
				else
					bittestandclear( d.var, 0 );

				if( c )
					d.var = ((d.var + (signextend<TInt64,24>(s24.var) << 24) )&0xffffffffff000000) | (d.var & 0xffffff);
				else
					d.var = ((d.var - (signextend<TInt64,24>(s24.var) << 24) )&0xffffffffff000000) | (d.var & 0xffffff);

				sr_toggle( SR_C, bittest(d,55) == 0 );
				sr_toggle( SR_V, changed47 );
				sr_l_update_by_v();

				d.var &= 0x00ffffffffffffff;

	//			LOG( "DIV: d" << std::hex << debugOldD.var << " s" << std::hex << debugOldS.var << " =>" << std::hex << d.var );				
			}
			return true;
		// Double-Precision Multiply-Accumulate With Right Shift
		case OpcodeInfo::Dmac:	// 000000010010010s1SdkQQQQ
			{
				const bool dstUnsigned	= oi->getFieldValue(OpcodeInfo::Field_s, op);
				const bool srcUnsigned	= oi->getFieldValue(OpcodeInfo::Field_S, op);
				const bool ab			= oi->getFieldValue(OpcodeInfo::Field_d, op);
				const bool negate		= oi->getFieldValue(OpcodeInfo::Field_k, op);

				const TWord qqqq		= oi->getFieldValue(OpcodeInfo::Field_QQQQ, op);

				TReg24 s1, s2;
				decode_QQQQ_read( s1, s2, qqqq );

				alu_dmac( ab, s1, s2, negate, srcUnsigned, dstUnsigned );
			}
			return true;
		// Start Hardware Loop
		case OpcodeInfo::Do_ea:
		case OpcodeInfo::Do_aa:
			LOG_ERR_NOTIMPLEMENTED("DO");
			return true;
		case OpcodeInfo::Do_xxx:	// 00000110iiiiiiii1000hhhh
			{
				const TWord addr = fetchPC();
				TWord loopcount = oi->getFieldValue(OpcodeInfo::Field_hhhh, OpcodeInfo::Field_iiiiiiii, op);

				exec_do( TReg24(loopcount), addr );				
			}
			return true;
		case OpcodeInfo::Do_S:		// 0000011011DDDDDD00000000
			{
				const TWord addr = fetchPC();

				const TWord dddddd = oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

				const TReg24 loopcount = decode_dddddd_read( dddddd );

				exec_do( loopcount, addr );
			}
			return true;
		case OpcodeInfo::DoForever:
			LOG_ERR_NOTIMPLEMENTED("DO FOREVER");
			return true;
		// Start Infinite Loop
		case OpcodeInfo::Dor_ea:	// 0000011001MMMRRR0S010000
		case OpcodeInfo::Dor_aa:	// 0000011000aaaaaa0S010000
			LOG_ERR_NOTIMPLEMENTED("DOR");
			return true;
		case OpcodeInfo::Dor_xxx:	// 00000110iiiiiiii1001hhhh
            {
	            const auto loopcount = oi->getFieldValue(OpcodeInfo::Field_hhhh, OpcodeInfo::Field_iiiiiiii, op);
                const auto displacement = signextend<int, 24>(fetchPC());
                exec_do(TReg24(loopcount), reg.pc.var + displacement - 2);
            }
            return true;
		case OpcodeInfo::Dor_S:		// 00000110 11DDDDDD 00010000
			{
				const TWord dddddd = oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);
				const TReg24 lc		= decode_dddddd_read( dddddd );
				
				const int displacement = signextend<int,24>(fetchPC());
				exec_do( lc, reg.pc.var + displacement - 2 );
			}
			return true;
		// Start PC-Relative Infinite Loops
		case OpcodeInfo::DorForever:
			LOG_ERR_NOTIMPLEMENTED("DOR FOREVER");
			return true;
		// Load PC-Relative Address
		case OpcodeInfo::Lra_Rn:
		case OpcodeInfo::Lra_xxxx:
			LOG_ERR_NOTIMPLEMENTED("LRA");
			return true;
		// Load Updated Address
		case OpcodeInfo::Lua_ea:	// 00000100010MMRRR000ddddd
			{
				const TWord mmrrr	= oi->getFieldValue(OpcodeInfo::Field_MM, OpcodeInfo::Field_RRR, op);
				const TWord ddddd	= oi->getFieldValue(OpcodeInfo::Field_ddddd, op);

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
			return true;
		case OpcodeInfo::Lua_Rn:	// 00000100 00aaaRRR aaaadddd
			{
				const TWord dddd	= oi->getFieldValue(OpcodeInfo::Field_dddd, op);
				const TWord a		= oi->getFieldValue(OpcodeInfo::Field_aaa, OpcodeInfo::Field_aaaa, op);
				const TWord rrr		= oi->getFieldValue(OpcodeInfo::Field_RRR, op);

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
			return true;
		// Signed Multiply Accumulate
		case OpcodeInfo::Mac_S:	// 00000001 000sssss 11QQdk10
			{
				const TWord sssss	= oi->getFieldValue(OpcodeInfo::Field_sssss, op);
				const TWord qq		= oi->getFieldValue(OpcodeInfo::Field_QQ, op);
				const bool	ab		= oi->getFieldValue(OpcodeInfo::Field_d, op);
				const bool	negate	= oi->getFieldValue(OpcodeInfo::Field_k, op);

				const TReg24 s1 = decode_QQ_read( qq );
				const TReg24 s2( decode_sssss(sssss) );

				alu_mac( ab, s1, s2, negate, false );
			}
			return true;
		// Signed Multiply Accumulate With Immediate Operand
		case OpcodeInfo::Maci_xxxx:
			LOG_ERR_NOTIMPLEMENTED("MACI");
			return true;
		// Mixed Multiply Accumulate
		case OpcodeInfo::Macsu:	// 0 0 0 0 0 0 0 1 0 0 1 0 0 1 1 0 1 s d k Q Q Q Q
			{
				const bool uu			= oi->getFieldValue(OpcodeInfo::Field_s, op);
				const bool ab			= oi->getFieldValue(OpcodeInfo::Field_d, op);
				const bool negate		= oi->getFieldValue(OpcodeInfo::Field_k, op);
				const TWord qqqq		= oi->getFieldValue(OpcodeInfo::Field_QQQQ, op);

				TReg24 s1, s2;
				decode_QQQQ_read( s1, s2, qqqq );

				alu_mac( ab, s1, s2, negate, uu );
			}
			return true;
		// Signed Multiply Accumulate and Round
		case OpcodeInfo::Macr_S:
			LOG_ERR_NOTIMPLEMENTED("MACR");
			return true;
		// Signed MAC and Round With Immediate Operand
		case OpcodeInfo::Macri_xxxx:
			LOG_ERR_NOTIMPLEMENTED("MACRI");
			return true;
		// Signed Multiply
		case OpcodeInfo::Mpy_SD:	// 00000001000sssss11QQdk00
			{
				const int sssss		= oi->getFieldValue(OpcodeInfo::Field_sssss, op);
				const TWord QQ		= oi->getFieldValue(OpcodeInfo::Field_QQ, op);
				const bool ab		= oi->getFieldValue(OpcodeInfo::Field_d, op);
				const bool negate	= oi->getFieldValue(OpcodeInfo::Field_k, op);

				const TReg24 s1 = decode_QQ_read(QQ);

				const TReg24 s2 = TReg24( decode_sssss(sssss) );

				alu_mpy( ab, s1, s2, negate, false );
			}
			return true;
		// Mixed Multiply
		case OpcodeInfo::Mpy_su:	// 00000001001001111sdkQQQQ
			{
				const bool ab		= oi->getFieldValue(OpcodeInfo::Field_d, op);
				const bool negate	= oi->getFieldValue(OpcodeInfo::Field_k, op);
				const bool uu		= oi->getFieldValue(OpcodeInfo::Field_s, op);
				const TWord qqqq	= oi->getFieldValue(OpcodeInfo::Field_QQQQ, op);

				TReg24 s1, s2;
				decode_QQQQ_read( s1, s2, qqqq );

				alu_mpysuuu( ab, s1, s2, negate, false, uu );
			}
			return true;
		// Signed Multiply With Immediate Operand
		case OpcodeInfo::Mpyi:		// 00000001 01000001 11qqdk00
			{
				const bool	ab		= oi->getFieldValue(OpcodeInfo::Field_d, op);
				const bool	negate	= oi->getFieldValue(OpcodeInfo::Field_k, op);
				const TWord qq		= oi->getFieldValue(OpcodeInfo::Field_QQ, op);

				const TReg24 s		= TReg24(fetchPC());

				const TReg24 reg	= decode_qq_read(qq);

				alu_mpy( ab, reg, s, negate, false );
			}
			return true;
		// Signed Multiply and Round
		case OpcodeInfo::Mpyr_SD:
			LOG_ERR_NOTIMPLEMENTED("MPYR");
			return true;
		// Signed Multiply and Round With Immediate Operand
		case OpcodeInfo::Mpyri:
			LOG_ERR_NOTIMPLEMENTED("MPYRI");
			return true;
		// Norm Accumulator Iterations
		case OpcodeInfo::Norm:
			LOG_ERR_NOTIMPLEMENTED("NORM");
			return true;
		// Fast Accumulator Normalization
		case OpcodeInfo::Normf:
			LOG_ERR_NOTIMPLEMENTED("NORMF");
			return true;
		// Lock Instruction Cache Sector
		case OpcodeInfo::Plock:
			cache.plock( fetchPC() );
			return true;
		case OpcodeInfo::Punlock:
			LOG_ERR_NOTIMPLEMENTED("PUNLOCK");
			return true;
		// Transfer Conditionally
		case OpcodeInfo::Tcc_S1D1:	// Tcc S1,D1 - 00000010 CCCC0000 0JJJd000
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc( cccc ) )
				{
					const TWord JJJ = oi->getFieldValue(OpcodeInfo::Field_JJJ, op);
					const bool ab = oi->getFieldValue(OpcodeInfo::Field_d, op);

					decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
				}
			}
			return true;
		case OpcodeInfo::Tcc_S1D1S2D2:	// Tcc S1,D1 S2,D2 - 00000011 CCCC0ttt 0JJJdTTT
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc( cccc ) )
				{
					const TWord TTT		= oi->getFieldValue(OpcodeInfo::Field_TTT, op);
					const TWord JJJ		= oi->getFieldValue(OpcodeInfo::Field_JJJ, op);
					const TWord ttt		= oi->getFieldValue(OpcodeInfo::Field_ttt, op);
					const bool ab		= oi->getFieldValue(OpcodeInfo::Field_d, op);

					decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
					reg.r[TTT] = reg.r[ttt];
				}
			}
			return true;
		case OpcodeInfo::Tcc_S2D2:	// Tcc S2,D2 - 00000010 CCCC1ttt 00000TTT
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc( cccc ) )
				{
					const TWord TTT		= oi->getFieldValue(OpcodeInfo::Field_TTT, op);
					const TWord ttt		= oi->getFieldValue(OpcodeInfo::Field_ttt, op);
					reg.r[TTT] = reg.r[ttt];
				}
			}
			return true;
		// Viterbi Shift Left
		case OpcodeInfo::Vsl:	// VSL S,i,L:ea - 0 0 0 0 1 0 1 S 1 1 M M M R R R 1 1 0 i 0 0 0 0
			LOG_ERR_NOTIMPLEMENTED("VSL");
			return true;
		default:
			return false;
		}
	}

	// _____________________________________________________________________________
	// exec_pcu
	//
	bool DSP::exec_pcu(const OpcodeInfo* oi, TWord op)
	{
		switch (oi->getInstruction())
		{
		// Branch conditionally
		case OpcodeInfo::Bcc_xxxx:	// TODO: unclear documentation, opcode that is written there is wrong
			return false;
		case OpcodeInfo::Bcc_xxx:	// 00000101 CCCC01aa aa0aaaaa
			{
				const TWord cccc	= oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc( cccc ) )
				{
					const TWord a		= oi->getFieldValue(OpcodeInfo::Field_aaaa, OpcodeInfo::Field_aaaaa, op);

					const TWord disp	= signextend<int,9>( a );
					assert( disp >= 0 );

					reg.pc.var += (disp-1);
				}
			}
			return true;
		case OpcodeInfo::Bcc_Rn:		// 00001101 00011RRR 0100CCCC
			LOG_ERR_NOTIMPLEMENTED("Bcc Rn");
			return true;
		// Branch always
		case OpcodeInfo::Bra_xxxx:
			{
				const int displacement = signextend<int,24>( fetchPC() );
				reg.pc.var += displacement - 2;				
			}
			return true;
		case OpcodeInfo::Bra_xxx:		// 00000101 000011aa aa0aaaaa
			{
				const TWord addr = oi->getFieldValue(OpcodeInfo::Field_aaaa, OpcodeInfo::Field_aaaaa, op);

				const int signedAddr = signextend<int,9>(addr);

				reg.pc.var += (signedAddr-1);	// PC has already been incremented so subtract 1
			}
			return true;
		case OpcodeInfo::Bra_Rn:		// 0000110100011RRR11000000
			LOG_ERR_NOTIMPLEMENTED("BRA_Rn");
			return true;
		// Branch if Bit Clear
		case OpcodeInfo::Brclr_ea:	// BRCLR #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 0 b b b b b
		case OpcodeInfo::Brclr_aa:	// BRCLR #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 0 b b b b b
		case OpcodeInfo::Brclr_pp:	// BRCLR #n,[X or Y]:pp,xxxx - 0 0 0 0 1 1 0 0 1 1 p p p p p p 0 S 0 b b b b b
			LOG_ERR_NOTIMPLEMENTED("BRCLR");			
			return true;
		case OpcodeInfo::Brclr_qq:
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord qqqqqq	= oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord ea = 0xffff80 + qqqqqq;

				const int displacement = signextend<int,24>( fetchPC() );

				if( !bittest( memRead( S, ea ), bit ) )
				{
					reg.pc.var += (displacement-2);
				}
			}
			return true;
		case OpcodeInfo::Brclr_S:	// BRCLR #n,S,xxxx - 0 0 0 0 1 1 0 0 1 1 D D D D D D 1 0 0 b b b b b
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

				const TReg24 tst = decode_dddddd_read( dddddd );

				const int displacement = signextend<int,24>( fetchPC() );

				if( !bittest( tst, bit ) )
				{
					reg.pc.var += (displacement-2);
				}
			}
			return true;
		// Branch if Bit Set
		case OpcodeInfo::Brset_ea:	// BRSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 1 b b b b b
		case OpcodeInfo::Brset_aa:	// BRSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 1 b b b b b
		case OpcodeInfo::Brset_pp:	// BRSET #n,[X or Y]:pp,xxxx - 
			LOG_ERR_NOTIMPLEMENTED("BRSET");
			return true;
		case OpcodeInfo::Brset_qq:
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord qqqqqq	= oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord ea = 0xffff80 + qqqqqq;

				const int displacement = signextend<int,24>( fetchPC() );

				if( bittest( memRead( S, ea ), bit ) )
				{
					reg.pc.var += (displacement-2);
				}
			}
			return true;
		case OpcodeInfo::Brset_S:
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

				const TReg24 r = decode_dddddd_read( dddddd );

				const int displacement = signextend<int,24>( fetchPC() );

				if( bittest( r.var, bit ) )
				{
					reg.pc.var += (displacement-2);
				}				
			}
			return true;
		// Branch to Subroutine if Bit Clear
		case OpcodeInfo::Bsclr_ea:
		case OpcodeInfo::Bsclr_aa:
		case OpcodeInfo::Bsclr_qq:
		case OpcodeInfo::Bsclr_pp:
		case OpcodeInfo::Bsclr_S:
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
			return true;
		// Branch to Subroutine if Bit Set
		case OpcodeInfo::Bsset_ea:
		case OpcodeInfo::Bsset_aa:
		case OpcodeInfo::Bsset_qq:
		case OpcodeInfo::Bsset_pp:
		case OpcodeInfo::Bsset_S:
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
			return true;

		// Branch to Subroutine Conditionally
		case OpcodeInfo::BScc_xxxx:		// 00001101000100000000CCCC
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				const int displacement = signextend<int,24>(fetchPC());

				if( decode_cccc(cccc) )
				{
					jsr( reg.pc.var + displacement - 2 );
					LOGSC("BScc xxxx");
				}
			}
			return true;
		case OpcodeInfo::BScc_xxx:		// 00000101CCCC00aaaa0aaaaa
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc(cccc) )
				{
					const TWord addr = oi->getFieldValue(OpcodeInfo::Field_aaaa, OpcodeInfo::Field_aaaaa, op);

					int signedAddr = signextend<int,9>(addr);

					// PC has already been incremented so subtract 1
					jsr( reg.pc.var + signedAddr-1 );
					LOGSC("BScc xxx");
				}				
			}
			return true;
		case OpcodeInfo::BScc_Rn:
			LOG_ERR_NOTIMPLEMENTED("BScc Rn");
			return true;
		// Branch to Subroutine
		case OpcodeInfo::Bsr_xxxx:
			{
				const int displacement = signextend<int,24>(fetchPC());
				jsr( reg.pc.var + displacement - 2 );
			}
			return true;
		case OpcodeInfo::Bsr_xxx:		// 00000101000010aaaa0aaaaa
			{
				const TWord aaaaaaaaa = oi->getFieldValue(OpcodeInfo::Field_aaaa, OpcodeInfo::Field_aaaaa, op);

				const int shortDisplacement = signextend<int,9>(aaaaaaaaa);

				jsr( TReg24(reg.pc.var + shortDisplacement - 1) );
				LOGSC("bsr xxx");
			}
			return true;
		case OpcodeInfo::Bsr_Rn:  // 0000110100011RRR10000000
            {
                const auto rrr = oi->getFieldValue(OpcodeInfo::Field_RRR, op);
                jsr( TReg24(reg.pc.var + reg.r[rrr].var - 1) );
            }
            return true;
		case OpcodeInfo::Debug:
			LOG( "Entering DEBUG mode" );
			LOG_ERR_NOTIMPLEMENTED("DEBUG");
			return true;
		case OpcodeInfo::Debugcc:
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);
				if( decode_cccc( cccc ) )
				{
					LOG( "Entering DEBUG mode because condition is met" );
				}
				LOG_ERR_NOTIMPLEMENTED("DEBUGcc");
			}
			return true;
		// Jump Conditionally
		case OpcodeInfo::Jcc_xxx:		// 00001110CCCCaaaaaaaaaaaa
			{
				const TWord cccc = oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc( cccc ) )
				{
					const TWord ea = oi->getFieldValue(OpcodeInfo::Field_aaaaaaaaaaaa, op);
					setPC(ea);
				}				
			}
			return true;
		case OpcodeInfo::Jcc_ea:
			{
				const TWord cccc	= oi->getFieldValue(OpcodeInfo::Field_CCCC, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);

				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				if( decode_cccc( cccc ) )
				{
					setPC(ea);
				}
			}
			return true;
		// Jump
		case OpcodeInfo::Jmp_ea:
			{
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				setPC(decode_MMMRRR_read(mmmrrr));
			}
			return true;
		case OpcodeInfo::Jmp_xxx:	// 00001100 0000aaaa aaaaaaaa
			setPC(oi->getFieldValue(OpcodeInfo::Field_aaaaaaaaaaaa, op));
			return true;
		// Jump to Subroutine Conditionally
		case OpcodeInfo::Jscc_xxx:
			{
				const TWord cccc	= oi->getFieldValue(OpcodeInfo::Field_CCCC, op);

				if( decode_cccc( cccc ) )
				{
					const TWord a = oi->getFieldValue(OpcodeInfo::Field_aaaaaaaaaaaa, op);
					jsr(a);
				}
			}
			return true;
		case OpcodeInfo::Jscc_ea:
			{
				const TWord cccc	= oi->getFieldValue(OpcodeInfo::Field_CCCC, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				if( decode_cccc( cccc ) )
				{
					jsr(TReg24(ea));
					LOGSC("JScc ea");
				}
			}
			return true;
		case OpcodeInfo::Jsclr_ea:
		case OpcodeInfo::Jsclr_aa:
		case OpcodeInfo::Jsclr_pp:
		case OpcodeInfo::Jsclr_qq:
		case OpcodeInfo::Jsclr_S:
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
			return true;
		// Jump if Bit Clear
		case OpcodeInfo::Jclr_ea:	// 0000101001MMMRRR1S0bbbbb
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;
				const TWord addr	= fetchPC();

				const TWord ea		= decode_MMMRRR_read(mmmrrr);

				if( !bittest( ea, bit ) )	// TODO: S is not used, need to read mem if mmmrrr is not immediate data!
				{
					setPC(addr);
				}

				LOG_ERR_NOTIMPLEMENTED("JCLR");
			}
			return true;
		case OpcodeInfo::Jclr_aa:
			LOG_ERR_NOTIMPLEMENTED("JCLR aa");
			return true;
		case OpcodeInfo::Jclr_pp:
			LOG_ERR_NOTIMPLEMENTED("JCLR pp");
			return true;
		case OpcodeInfo::Jclr_qq:	// 00000001 10qqqqqq 1S0bbbbb
			{
				const TWord qqqqqq	= oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord ea		= 0xffff80 + qqqqqq;

				const TWord addr	= fetchPC();

				if( !bittest( memRead(S, ea), bit ) )
					setPC(addr);
			}
			return true;
		case OpcodeInfo::Jclr_S:	// 00001010 11DDDDDD 000bbbbb
			{
				const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);

				const TWord addr = fetchPC();

				if( !bittest( decode_dddddd_read(dddddd), bit ) )
					setPC(addr);
			}
			return true;
		// Jump if Bit Set
		case OpcodeInfo::Jset_ea:	// 0000101001MMMRRR1S1bbbbb
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord val		= memRead( S, decode_MMMRRR_read( mmmrrr ) );

				if( bittest(val,bit) )
				{
					setPC(val);
				}
			}
			return true;
		case OpcodeInfo::Jset_aa:	// JSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 0 1 0 0 0 a a a a a a 1 S 1 b b b b b
			LOG_ERR_NOTIMPLEMENTED("JSET #n,[X or Y]:aa,xxxx");
			return true;
		case OpcodeInfo::Jset_pp:	// JSET #n,[X or Y]:pp,xxxx - 0 0 0 0 1 0 1 0 1 0 p p p p p p 1 S 1 b b b b b
			{
				const TWord qqqqqq	= oi->getFieldValue(OpcodeInfo::Field_pppppp, op);
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord ea		= 0xFFFFC0 + qqqqqq;

				const TWord addr	= fetchPC();

				if( bittest( memRead(S, ea), bit ) )
					setPC(addr);
			}
			return true;
		case OpcodeInfo::Jset_qq:
			{
				// TODO: combine code with Jset_pp, only the offset is different
				const TWord qqqqqq	= oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord ea		= 0xFFFF80 + qqqqqq;

				const TWord addr	= fetchPC();

				if( bittest( memRead(S, ea), bit ) )
					setPC(addr);
			}
			return true;
		case OpcodeInfo::Jset_S:	// 0000101011DDDDDD001bbbbb
			{
				const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
				const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

				const TWord addr	= fetchPC();

				const TReg24 var	= decode_dddddd_read( dddddd );

				if( bittest(var,bit) )
				{
					setPC(addr);
				}				
			}
			return true;
		// Jump to Subroutine
		case OpcodeInfo::Jsr_ea:
			{
				const TWord mmmrrr = oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);

				const TWord ea = decode_MMMRRR_read( mmmrrr );

				jsr(TReg24(ea));
				LOGSC("jsr ea");
			}
			return true;
		case OpcodeInfo::Jsr_xxx:
			{
				const TWord ea = oi->getFieldValue(OpcodeInfo::Field_aaaaaaaaaaaa, op);
				jsr(TReg24(ea));
				LOGSC("jsr xxx");
			}
			return true;
		// Jump to Subroutine if Bit Set
		case OpcodeInfo::Jsset_ea:
		case OpcodeInfo::Jsset_aa:
		case OpcodeInfo::Jsset_pp:
		case OpcodeInfo::Jsset_qq:
		case OpcodeInfo::Jsset_S:
			LOG_ERR_NOTIMPLEMENTED("JSSET");
			return true;
		// Repeat Next Instruction
		case OpcodeInfo::Rep_ea:
		case OpcodeInfo::Rep_aa:
			LOG_ERR_NOTIMPLEMENTED("REP");
			return true;
		case OpcodeInfo::Rep_xxx:	// 00000110 iiiiiiii 1010hhhh
			{
				const TWord loopcount = oi->getFieldValue(OpcodeInfo::Field_hhhh, OpcodeInfo::Field_iiiiiiii, op);

				repRunning = true;
				tempLCforRep = reg.lc;

				reg.lc.var = loopcount;				
			}
			return true;
		case OpcodeInfo::Rep_S:
			LOG_ERR_NOTIMPLEMENTED("REP S");
			return true;
		// Reset On-Chip Peripheral Devices
		case OpcodeInfo::Reset:
			resetSW();
			return true;
		default:
			return false;
		}
	}

	// _____________________________________________________________________________
	// decode_MMMRRR
	//
	dsp56k::TWord DSP::decode_MMMRRR_read( TWord _mmmrrr )
	{
		switch( _mmmrrr & 0x3f )
		{
		case MMM_AbsAddr:		return fetchPC();		// absolute address
		case MMM_ImmediateData:	return fetchPC();		// immediate data
		}

		unsigned int regIdx = _mmmrrr & 0x7;

		const TReg24	_n = reg.n[regIdx];
		TReg24&			_r = reg.r[regIdx];
		const TReg24	_m = reg.m[regIdx];

		TWord r = _r.toWord();

		TWord a;

		switch( _mmmrrr & 0x38 )
		{
		case 0x00:	/* 000 */	a = r;					AGU::updateAddressRegister(r,-_n.var,_m.var);					break;
		case 0x08:	/* 001 */	a = r;					AGU::updateAddressRegister(r,+_n.var,_m.var);					break;
		case 0x10:	/* 010 */	a = r;					AGU::updateAddressRegister(r,-1,_m.var);						break;
		case 0x18:	/* 011 */	a =	r;					AGU::updateAddressRegister(r,+1,_m.var);						break;
		case 0x20:	/* 100 */	a = r;																					break;
		case 0x28:	/* 101 */	a = r + _n.toWord();																	break;
		case 0x38:	/* 111 */							AGU::updateAddressRegister(r,-1,_m.var);		a = _r.var;		break;

		default:
			assert(0 && "impossible to happen" );
			return 0xbadbad;
		}

		_r.var = r;

		assert( a >= 0x00000000 && a <= 0x00ffffff && "invalid memory address" );
		return a;
	}

	// _____________________________________________________________________________
	// decode_XMove_MMRRR
	//
	dsp56k::TWord DSP::decode_XMove_MMRRR( TWord _mmrrr )
	{
		unsigned int regIdx = _mmrrr & 0x07;

		const TReg24	_n = reg.n[regIdx];
		TReg24&			_r = reg.r[regIdx];
		const TReg24	_m = reg.m[regIdx];

		TWord r = _r.toWord();

		TWord a;

		switch( _mmrrr & 0x18 )
		{
		case 0x08:	/* 01 */	a = r;	AGU::updateAddressRegister(r,+_n.var,_m.var);	break;
		case 0x10:	/* 10 */	a = r;	AGU::updateAddressRegister(r,-1,_m.var);		break;
		case 0x18:	/* 11 */	a =	r;	AGU::updateAddressRegister(r,+1,_m.var);		break;
		case 0x00:	/* 00 */	a = r;													break;

		default:
			assert(0 && "impossible to happen" );
			return 0xbadbad;
		}

		_r.var = r;

		assert( a >= 0x00000000 && a <= 0x00ffffff && "invalid memory address" );
		return a;
	}
	// _____________________________________________________________________________
	// decode_YMove_mmrr
	//
	dsp56k::TWord DSP::decode_YMove_mmrr( TWord _mmrr, TWord _regIdxOffset )
	{
		unsigned int regIdx = _mmrr & 0x3;

		const TReg24	_n = reg.n[regIdx + _regIdxOffset];
		TReg24&			_r = reg.r[regIdx + _regIdxOffset];
		const TReg24	_m = reg.m[regIdx + _regIdxOffset];

		TWord r = _r.toWord();

		TWord a;

		switch( _mmrr & 0xc	 )
		{
		case 0x4:	/* 01 */	a = r;	AGU::updateAddressRegister(r,+_n.var,_m.var);	break;
		case 0x8:	/* 10 */	a = r;	AGU::updateAddressRegister(r,-1,_m.var);		break;
		case 0xc:	/* 11 */	a =	r;	AGU::updateAddressRegister(r,+1,_m.var);		break;
		case 0x0:	/* 00 */	a = r;													break;

		default:
			assert(0 && "impossible to happen" );
			return 0xbadbad;
		}

		_r.var = r;

		assert( a >= 0x00000000 && a <= 0x00ffffff && "invalid memory address" );
		return a;
	}

	// _____________________________________________________________________________
	// decode_RRR_read
	//
	dsp56k::TWord DSP::decode_RRR_read( TWord _mmmrrr, int _shortDisplacement )
	{
		unsigned int regIdx = _mmmrrr&0x07;

		const TReg24& _r = reg.r[regIdx];

		const int ea = _r.var + _shortDisplacement;

		return ea&0x00ffffff;
	}

// _____________________________________________________________________________
// exec_move
//
bool DSP::exec_move(const OpcodeInfo* oi, TWord op)
{
	switch (oi->getInstruction())
	{
		// X or Y Memory Data Move with immediate displacement
		case OpcodeInfo::Movex_Rnxxxx:	// 0000101001110RRR1WDDDDDD
		case OpcodeInfo::Movey_Rnxxxx:	// 0000101101110RRR1WDDDDDD
			{
				const TWord DDDDDD	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);
				const auto	write	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const TWord rrr		= oi->getFieldValue(OpcodeInfo::Field_RRR, op);

				const int shortDisplacement = signextend<int,24>(fetchPC());
				const TWord ea = decode_RRR_read( rrr, shortDisplacement );

				const auto area = oi->getInstruction() == OpcodeInfo::Movey_Rnxxxx ? MemArea_Y : MemArea_X;

				if( write )
				{
					decode_dddddd_write( DDDDDD, TReg24(memRead( area, ea )) );
				}
				else
				{
					memWrite( area, ea, decode_dddddd_read( DDDDDD ).var );
				}
			}
			return true;
		case OpcodeInfo::Movex_Rnxxx:	// 0000001aaaaaaRRR1a0WDDDD
		case OpcodeInfo::Movey_Rnxxx:	// 0000001aaaaaaRRR1a1WDDDD
			{
				const TWord ddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDD, op);
				const TWord aaaaaaa	= oi->getFieldValue(OpcodeInfo::Field_aaaaaa, OpcodeInfo::Field_a, op);
				const auto	write	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const TWord rrr		= oi->getFieldValue(OpcodeInfo::Field_RRR, op);

				const int shortDisplacement = signextend<int,7>(aaaaaaa);
				const TWord ea = decode_RRR_read( rrr, shortDisplacement );

				const auto area = oi->getInstruction() == OpcodeInfo::Movey_Rnxxx ? MemArea_Y : MemArea_X;

				if( write )
				{
					decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( area, ea )) );
				}
				else
				{
					memWrite( area, ea, decode_ddddd_read<TWord>( ddddd ) );
				}
			}
			return true;
		// Move Control Register
		case OpcodeInfo::Movec_ea:		// 00000101W1MMMRRR0S1DDDDD
			{
				const TWord ddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDD, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const auto write	= oi->getFieldValue(OpcodeInfo::Field_W, op);

				const TWord addr = decode_MMMRRR_read( mmmrrr );

				const EMemArea area = oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;
					
				if( write )
				{
					if( mmmrrr == MMM_ImmediateData )	decode_ddddd_pcr_write( ddddd, TReg24(addr) );		
					else								decode_ddddd_pcr_write( ddddd, TReg24(memRead( area, addr )) );
				}
				else
				{
					const TReg24 regVal = decode_ddddd_pcr_read(op&0x1f);
					assert( (mmmrrr != MMM_ImmediateData) && "register move to immediate data? not possible" );
					memWrite( area, addr, regVal.toWord() );
				}
			}
			return true;
		case OpcodeInfo::Movec_aa:
			LOG_ERR_NOTIMPLEMENTED("MOVE(C)_aa");
			return true;
		case OpcodeInfo::Movec_S1D2:	// 00000100W1eeeeee101ddddd
			{
				const auto write = oi->getFieldValue(OpcodeInfo::Field_W, op);

				const TWord eeeeee	= oi->getFieldValue(OpcodeInfo::Field_eeeeee, op);
				const TWord ddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDD, op);

				if( write )
					decode_ddddd_pcr_write( ddddd, decode_dddddd_read( eeeeee ) );
				else
					decode_dddddd_write( eeeeee, decode_ddddd_pcr_read( ddddd ) );				
			}
			return true;
		case OpcodeInfo::Movec_xx:		// 00000101iiiiiiii101ddddd
			{
				const TWord iiiiiiii	= oi->getFieldValue(OpcodeInfo::Field_iiiiiiii, op);
				const TWord ddddd		= oi->getFieldValue(OpcodeInfo::Field_DDDDD, op);
				decode_ddddd_pcr_write( ddddd, TReg24(iiiiiiii) );
			}
			return true;
		// Move Program Memory
		case OpcodeInfo::Movem_ea:		// 00000111W1MMMRRR10dddddd
			{
				const auto	write	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_dddddd, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);

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
			return true;
		case OpcodeInfo::Movem_aa:		// 00000111W0aaaaaa00dddddd
			LOG_ERR_NOTIMPLEMENTED("MOVE(M) S,P:aa");
			return true;
		// Move Peripheral Data
		case OpcodeInfo::Movep_ppea:	// 0000100sW1MMMRRR1Spppppp
			{
				const TWord pp		= 0xffffc0 + oi->getFieldValue(OpcodeInfo::Field_pppppp, op);
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const auto write	= oi->getFieldValue(OpcodeInfo::Field_W, op);
				const EMemArea s	= oi->getFieldValue(OpcodeInfo::Field_s, op) ? MemArea_Y : MemArea_X;
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				if( write )
				{
					if( mmmrrr == MMM_ImmediateData )
						memWrite( S, pp, ea );
					else
						memWrite( S, pp, memRead( s, ea ) );
				}
				else
					memWrite( s, ea, memRead( S, pp ) );
			}
			return true;
		case OpcodeInfo::Movep_Xqqea:	// 00000111W1MMMRRR0Sqqqqqq
		case OpcodeInfo::Movep_Yqqea:	// 00000111W0MMMRRR1Sqqqqqq
			{
				const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
				const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;
				const TWord qAddr	= 0xffff80 + oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);
				const auto write	= oi->getFieldValue(OpcodeInfo::Field_W, op);

				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				const auto area = oi->getInstruction() == OpcodeInfo::Movep_Yqqea ? MemArea_Y : MemArea_X;

				if( write )
				{
					if( mmmrrr == MMM_ImmediateData )
						memWrite( area, qAddr, ea );
					else
						memWrite( area, qAddr, memRead( S, ea ) );
				}
				else
					memWrite( S, ea, memRead( area, qAddr ) );				
			}
			return true;
		case OpcodeInfo::Movep_eapp:	// 0000100sW1MMMRRR01pppppp
			LOG_ERR_NOTIMPLEMENTED("MOVE");
			return true;

		case OpcodeInfo::Movep_eaqq:	// 000000001WMMMRRR0Sqqqqqq
			LOG_ERR_NOTIMPLEMENTED("MOVE");
			return true;
		case OpcodeInfo::Movep_Spp:		// 0000100sW1dddddd00pppppp
			{
				const TWord addr	= 0xffffc0 + oi->getFieldValue(OpcodeInfo::Field_pppppp, op);
				const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_dddddd, op);
				const EMemArea area = oi->getFieldValue(OpcodeInfo::Field_s, op) ? MemArea_Y : MemArea_X;
				const auto	write	= oi->getFieldValue(OpcodeInfo::Field_W, op);

				if( write )
					decode_dddddd_write( dddddd, TReg24(memRead( area, addr )) );
				else
					memWrite( area, addr, decode_dddddd_read( dddddd ).toWord() );
			}
		case OpcodeInfo::Movep_SXqq:	// 00000100W1dddddd1q0qqqqq
			LOG_ERR_NOTIMPLEMENTED("MOVE");
			return true;
		case OpcodeInfo::Movep_SYqq:	// 00000100W1dddddd0q1qqqqq
			LOG_ERR_NOTIMPLEMENTED("MOVE");
			return true;
		default:
			return false;
	}
}
// _____________________________________________________________________________
// decode_ddddd_pcr
//
dsp56k::TReg24 DSP::decode_ddddd_pcr_read( TWord _ddddd )
{
	if( (_ddddd & 0x18) == 0x00 )
	{
		return reg.m[_ddddd&0x07];
	}

	switch( _ddddd )
	{
	case 0xa:	return reg.ep;
	case 0x10:	return reg.vba;
	case 0x11:	return TReg24(reg.sc.var);
	case 0x18:	return reg.sz;
	case 0x19:	return reg.sr;
	case 0x1a:	return reg.omr;
	case 0x1b:	return reg.sp;
	case 0x1c:	return ssh();
	case 0x1d:	return ssl();
	case 0x1e:	return reg.la;
	case 0x1f:	return reg.lc;
	}
	assert( 0 && "invalid ddddd value" );
	return TReg24(0xbadbad);
}
// _____________________________________________________________________________
// decode_ddddd_pcr_write
//
void DSP::decode_ddddd_pcr_write( TWord _ddddd, TReg24 _val )
{
	if( (_ddddd & 0x18) == 0x00 )
	{
		reg.m[_ddddd&0x07] = _val;
		return;
	}

	switch( _ddddd )
	{
	case 0xa:	reg.ep = _val;				return;
	case 0x10:	reg.vba = _val;				return;
	case 0x11:	reg.sc.var = _val.var&0x1f;	return;
	case 0x18:	reg.sz = _val;				return;
	case 0x19:	reg.sr = _val;				return;
	case 0x1a:	reg.omr = _val;				return;
	case 0x1b:	reg.sp = _val;				return;
	case 0x1c:	ssh(_val);					return;
	case 0x1d:	ssl(_val);					return;
	case 0x1e:	reg.la = _val;				return;
	case 0x1f:	reg.lc = _val;				return;
	}
	assert( 0 && "unreachable" );
}

// _____________________________________________________________________________
// decode_eeeeee_read
//
dsp56k::TReg24 DSP::decode_dddddd_read( TWord _dddddd )
{
	switch( _dddddd & 0x3f )
	{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
	case 0x00:	return x0();
	case 0x01:	return x1();
	case 0x02:	return y0();
	case 0x03:	return y1();
		// 0001DD - 4 registers in data ALU
	case 0x04:	return x0();
	case 0x05:	return x1();
	case 0x06:	return y0();
	case 0x07:	return y1();

		// 001DDD - 8 accumulators in data ALU
	case 0x08:	return a0();
	case 0x09:	return b0();
	case 0x0a:	{ TReg24 res; convertS(res,a2()); return res; }
	case 0x0b:	{ TReg24 res; convertS(res,b2()); return res; }
	case 0x0c:	return a1();
	case 0x0d:	return b1();
	case 0x0e:	return getA<TReg24>();
	case 0x0f:	return getA<TReg24>();

		// 010TTT - 8 address registers in AGU
	case 0x10:	return reg.r[0];
	case 0x11:	return reg.r[1];
	case 0x12:	return reg.r[2];
	case 0x13:	return reg.r[3];
	case 0x14:	return reg.r[4];
	case 0x15:	return reg.r[5];
	case 0x16:	return reg.r[6];
	case 0x17:	return reg.r[7];

		// 011NNN - 8 address offset registers in AGU
	case 0x18:	return reg.n[0];
	case 0x19:	return reg.n[1];
	case 0x1a:	return reg.n[2];
	case 0x1b:	return reg.n[3];
	case 0x1c:	return reg.n[4];
	case 0x1d:	return reg.n[5];
	case 0x1e:	return reg.n[6];
	case 0x1f:	return reg.n[7];

		// 100FFF - 8 address modifier registers in AGU
	case 0x20:	return reg.m[0];
	case 0x21:	return reg.m[1];
	case 0x22:	return reg.m[2];
	case 0x23:	return reg.m[3];
	case 0x24:	return reg.m[4];
	case 0x25:	return reg.m[5];
	case 0x26:	return reg.m[6];
	case 0x27:	return reg.m[7];

		// 101EEE - 1 adress register in AGU
	case 0x2a:	return reg.ep;

		// 110VVV - 2 program controller registers
	case 0x30:	return reg.vba;
	case 0x31:	return TReg24(reg.sc.var);

		// 111GGG - 8 program controller registers
	case 0x38:	return reg.sz;
	case 0x39:	return reg.sr;
	case 0x3a:	return reg.omr;
	case 0x3b:	return reg.sp;
	case 0x3c:	return ssh();
	case 0x3d:	return ssl();
	case 0x3e:	return reg.la;
	case 0x3f:	return reg.lc;
	}
	assert(0 && "invalid eeeeee value");
	return TReg24(0xbadbad);
}

// _____________________________________________________________________________
// decode_eeeeee_write
//
void DSP::decode_dddddd_write( TWord _dddddd, TReg24 _val )
{
	assert( (_val.var & 0xff000000) == 0 );

	switch( _dddddd & 0x3f )
	{
		// 0001DD - 4 registers in data ALU
	case 0x04:	x0(_val);	return;
	case 0x05:	x1(_val);	return;
	case 0x06:	y0(_val);	return;
	case 0x07:	y1(_val);	return;

		// 001DDD - 8 accumulators in data ALU
	case 0x08:	a0(_val);	return;
	case 0x09:	b0(_val);	return;
	case 0x0a:	{ TReg8 temp; convert(temp,_val); a2(temp);	return; }
	case 0x0b:	{ TReg8 temp; convert(temp,_val); b2(temp);	return; }
	case 0x0c:	a1(_val);	return;
	case 0x0d:	b1(_val);	return;
	case 0x0e:	setA(_val);	return;
	case 0x0f:	setB(_val);	return;

		// 010TTT - 8 address registers in AGU
	case 0x10:	reg.r[0] = _val;	return;
	case 0x11:	reg.r[1] = _val;	return;
	case 0x12:	reg.r[2] = _val;	return;
	case 0x13:	reg.r[3] = _val;	return;
	case 0x14:	reg.r[4] = _val;	return;
	case 0x15:	reg.r[5] = _val;	return;
	case 0x16:	reg.r[6] = _val;	return;
	case 0x17:	reg.r[7] = _val;	return;

		// 011NNN - 8 address offset registers in AGU
	case 0x18:	reg.n[0] = _val;	return;
	case 0x19:	reg.n[1] = _val;	return;
	case 0x1a:	reg.n[2] = _val;	return;
	case 0x1b:	reg.n[3] = _val;	return;
	case 0x1c:	reg.n[4] = _val;	return;
	case 0x1d:	reg.n[5] = _val;	return;
	case 0x1e:	reg.n[6] = _val;	return;
	case 0x1f:	reg.n[7] = _val;	return;

		// 100FFF - 8 address modifier registers in AGU
	case 0x20:	reg.m[0] = _val;	return;
	case 0x21:	reg.m[1] = _val;	return;
	case 0x22:	reg.m[2] = _val;	return;
	case 0x23:	reg.m[3] = _val;	return;
	case 0x24:	reg.m[4] = _val;	return;
	case 0x25:	reg.m[5] = _val;	return;
	case 0x26:	reg.m[6] = _val;	return;
	case 0x27:	reg.m[7] = _val;	return;

		// 101EEE - 1 adress register in AGU
	case 0x2a:	reg.ep = _val;		return;

		// 110VVV - 2 program controller registers
	case 0x30:	reg.vba = _val;			return;
	case 0x31:	reg.sc.var = _val.var & 0x1f;	return;

		// 111GGG - 8 program controller registers
	case 0x38:	reg.sz = _val;	return;
	case 0x39:	reg.sr = _val;	return;
	case 0x3a:	reg.omr = _val;	return;
	case 0x3b:	reg.sp = _val;	return;
	case 0x3c:	ssh(_val);	return;
	case 0x3d:	ssl(_val);	return;
	case 0x3e:	reg.la = _val;	return;
	case 0x3f:	reg.lc = _val;	return;
	}
	assert(0 && "unknown register");
}
// _____________________________________________________________________________
// logSC
//
void DSP::logSC( const char* _func ) const
{
	if( strstr(_func, "DO" ) )
		return;

	std::stringstream scss;
	
	for( int i=0; i<reg.sc.var; ++i )
	{
		scss << "\t";
	}

	LOG( std::string(scss.str()) << " SC=" << std::hex << (int)reg.sc.var << " pcOld=" << pcLastExec << " pcNew=" << reg.pc.var << " ictr=" << reg.ictr.var << " func=" << _func );
}
// _____________________________________________________________________________
// exec_operand_8bits
//
bool DSP::exec_operand_8bits(const OpcodeInfo* oi, TWord op)
{
	switch (oi->getInstruction())
	{
	case OpcodeInfo::Rti:			// Return From Interrupt
		popPCSR();
		LOGSC("rti");
		return true;
	case OpcodeInfo::Enddo:			// End Current DO Loop
		// restore previous loop flag
		sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

		// decrement SP twice, restoring old loop settings
		decSP();

		reg.lc = ssl();
		reg.la = ssh();

		return true;
	case OpcodeInfo::Dec:			// Decrement by One
		{
			auto& d = oi->getFieldValue(OpcodeInfo::Field_d, op) ? reg.b : reg.a;

			const auto old = d;
			const auto res = --d.var;

			d.doMasking();

			sr_s_update();
			sr_e_update(d);
			sr_u_update(d);
			sr_n_update_arithmetic(d);
			sr_z_update(d);
			sr_v_update(res,d);
			sr_l_update_by_v();
			sr_c_update_arithmetic(old,d);
			sr_toggle( SR_C, bittest(d,47) != bittest(old,47) );
		}
		return true;
	case OpcodeInfo::Inc:			// Increment by One	
		{
			auto& d = oi->getFieldValue(OpcodeInfo::Field_d, op) ? reg.b : reg.a;

			const auto old = d;

			const auto res = ++d.var;

			d.doMasking();

			sr_s_update();
			sr_e_update(d);
			sr_u_update(d);
			sr_n_update_arithmetic(d);
			sr_z_update(d);
			sr_v_update(res,d);
			sr_l_update_by_v();
			sr_c_update_arithmetic(old,d);
			sr_toggle( SR_C, bittest(d,47) != bittest(old,47) );
		}
		return true;
	case OpcodeInfo::Punlockr:
		LOG_ERR_NOTIMPLEMENTED("PUNLOCKR");
		return true;
	case OpcodeInfo::Pflush:
		LOG_ERR_NOTIMPLEMENTED("PFLUSH");
		return true;
	case OpcodeInfo::Pflushun:		// Program Cache Flush Unlocked Sections
		cache.pflushun();
		return true;
	case OpcodeInfo::Pfree:			// Program Cache Global Unlock
		cache.pfree();
		return true;
	case OpcodeInfo::Plockr:
		LOG_ERR_NOTIMPLEMENTED("PLOCKR");
		return true;
	case OpcodeInfo::Illegal:
		LOG_ERR_NOTIMPLEMENTED("ILLEGAL");
		return true;
	case OpcodeInfo::Rts:			// Return From Subroutine
		popPC();
		LOGSC( "RTS" );
		return true;
	case OpcodeInfo::Stop:
		LOG_ERR_NOTIMPLEMENTED("STOP");
		return true;
	case OpcodeInfo::Trap:
		LOG_ERR_NOTIMPLEMENTED("TRAP");
		return true;
	case OpcodeInfo::Wait:
		LOG_ERR_NOTIMPLEMENTED("WAIT");
		return true;
	case OpcodeInfo::Trapcc:
		LOG_ERR_NOTIMPLEMENTED("TRAPcc");
		return true;
	default:
		return false;
	}
}

// _____________________________________________________________________________
// resetSW
//
void DSP::resetSW()
{
	LOG_ERR_NOTIMPLEMENTED("RESET");
}

// _____________________________________________________________________________
// exec_move_ddddd_MMMRRR
//
void DSP::exec_move_ddddd_MMMRRR( TWord ddddd, TWord mmmrrr, bool write, EMemArea memArea )
{
	if( write && mmmrrr == MMM_ImmediateData )
	{
		decode_ddddd_write<TReg24>( ddddd, TReg24(fetchPC()) );
		return;
	}

	const TWord addr = decode_MMMRRR_read( mmmrrr );

	if( write )
	{
		decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( memArea, addr )) );
	}
	else
	{
		memWrite( memArea, addr, decode_ddddd_read<TWord>( ddddd ) );
	}
}

// _____________________________________________________________________________
// exec_bitmanip
//
bool DSP::exec_bitmanip(const OpcodeInfo* oi, TWord op)
{
	switch (oi->getInstruction())
	{
	case OpcodeInfo::Bset_ea:	// 0000101001MMMRRR0S1bbbbb
		// Bit Set and Test
		{
			const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
			const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

			const TWord ea		= decode_MMMRRR_read(mmmrrr);

			TWord val = memRead( S, ea );

			sr_toggle( SR_C, bittestandset( val, bit ) );

			memWrite( S, ea, val );
		}
		return true;
	case OpcodeInfo::Bset_aa:
	case OpcodeInfo::Bset_pp:
		LOG_ERR_NOTIMPLEMENTED("BSET");
		return true;
	case OpcodeInfo::Bset_qq:
		{
			const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord qqqqqq	= oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);
			const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

			const TWord ea		= 0xffff80 + qqqqqq;

			TWord val = memRead( S, ea );

			sr_toggle( SR_C, bittestandset( val, bit ) );

			memWrite( S, ea, val );			
		}
		return true;
	case OpcodeInfo::Bset_D:	// 0000101011DDDDDD011bbbbb
		{
			const TWord bit = oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord d = oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

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
		return true;
	// Bit test and change
	case OpcodeInfo::Bchg_ea:
	case OpcodeInfo::Bchg_aa:
	case OpcodeInfo::Bchg_pp:
	case OpcodeInfo::Bchg_qq:
		LOG_ERR_NOTIMPLEMENTED("BCHG");
		return true;
	case OpcodeInfo::Bchg_D:	// 00001011 11DDDDDD 010bbbbb
		{
			const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

			TReg24 val = decode_dddddd_read( dddddd );

			sr_toggle( SR_C, bittestandchange( val, bit ) );

			decode_dddddd_write( dddddd, val );

			sr_s_update();
			sr_l_update_by_v();
		}
		return true;
	// Bit test and clear
	case OpcodeInfo::Bclr_ea:
	case OpcodeInfo::Bclr_aa:
	case OpcodeInfo::Bclr_pp:
		LOG_ERR_NOTIMPLEMENTED("BCLR");
		return true;
	case OpcodeInfo::Bclr_qq:	// 0 0 0 0 0 0 0 1 0 0 q q q q q q 0 S 0 b b b b b
		{
			const TWord bit = oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord ea	= 0xffff80 + oi->getFieldValue(OpcodeInfo::Field_qqqqqq, op);

			const EMemArea S = oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

			const TWord res = alu_bclr( bit, memRead( S, ea ) );

			memWrite( S, ea, res );			
		}
		return true;
	case OpcodeInfo::Bclr_D:	// 0000101011DDDDDD010bbbbb
		{
			const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);

			TWord val;
			convert( val, decode_dddddd_read( dddddd ) );

			const TWord newVal = alu_bclr( bit, val );
			decode_dddddd_write( dddddd, TReg24(newVal) );			
		}
		return true;
	// Bit Test
	case OpcodeInfo::Btst_ea:
		{
			const TWord bit = oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord mmmrrr	= oi->getFieldValue(OpcodeInfo::Field_MMM, OpcodeInfo::Field_RRR, op);
			const TWord ea = decode_MMMRRR_read( mmmrrr );
			const EMemArea S = oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

			const TWord val = memRead( S, ea );

			sr_toggle( SR_C, bittest( val, bit ) );

			sr_s_update();
			sr_l_update_by_v();
		}
		return true;
	case OpcodeInfo::Btst_aa:
		LOG_ERR_NOTIMPLEMENTED("BTST aa");
		return true;
	case OpcodeInfo::Btst_pp:	// 0 0 0 0 1 0 1 1 1 0 p p p p p p 0 S 1 b b b b b
		{
			const TWord bitNum	= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);
			const TWord pppppp	= oi->getFieldValue(OpcodeInfo::Field_pppppp, op);
			const EMemArea S	= oi->getFieldValue(OpcodeInfo::Field_S, op) ? MemArea_Y : MemArea_X;

			const TWord memVal	= memRead( S, pppppp + 0xffffc0 );

			const bool bitSet	= ( memVal & (1<<bitNum)) != 0;

			sr_toggle( SR_C, bitSet );
		}
		return true;
	case OpcodeInfo::Btst_qq:	// 0 0 0 0 0 0 0 1 0 1 q q q q q q 0 S 1 b b b b b
		LOG_ERR_NOTIMPLEMENTED("BTST qq");
		return true;
	case OpcodeInfo::Btst_D:	// 0000101111DDDDDD011bbbbb
		{
			const TWord dddddd	= oi->getFieldValue(OpcodeInfo::Field_DDDDDD, op);
			const TWord bit		= oi->getFieldValue(OpcodeInfo::Field_bbbbb, op);

			TReg24 val = decode_dddddd_read( dddddd );

			sr_toggle( SR_C, bittest( val.var, bit ) );
		}
		return true;
	default:
		return false;
	}
}

// _____________________________________________________________________________
// exec_logical_nonparallel
//
bool DSP::exec_logical_nonparallel(const OpcodeInfo* oi, TWord op)
{
	switch (oi->getInstruction())
	{
	// Logical AND
	case OpcodeInfo::And_xx:	// 0000000101iiiiii10ood110
		{
			const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);
			const TWord xxxx = oi->getFieldValue(OpcodeInfo::Field_iiiiii, op);

			alu_and(ab, xxxx );
		}
		return true;
	case OpcodeInfo::And_xxxx:
		{
			const auto ab = oi->getFieldValue(OpcodeInfo::Field_d, op);
			const TWord xxxx = fetchPC();

			alu_and( ab, xxxx );
		}
		return true;
	// AND Immediate With Control Register
	case OpcodeInfo::Andi:	// AND(I) #xx,D		- 00000000 iiiiiiii 101110EE
		{
			const TWord ee		= oi->getFieldValue(OpcodeInfo::Field_EE, op);
			const TWord iiiiii	= oi->getFieldValue(OpcodeInfo::Field_iiiiiiii, op);

			TReg8 val = decode_EE_read(ee);
			val.var &= iiiiii;
			decode_EE_write(ee,val);			
		}
		return true;
	// Arithmetic Shift Accumulator Left
	case OpcodeInfo::Asl_ii:	// 00001100 00011101 SiiiiiiD
		{
			const TWord shiftAmount	= oi->getFieldValue(OpcodeInfo::Field_iiiiii, op);

			const bool abDst		= oi->getFieldValue(OpcodeInfo::Field_D, op);
			const bool abSrc		= oi->getFieldValue(OpcodeInfo::Field_S, op);

			alu_asl( abDst, abSrc, shiftAmount );			
		}
		return true;
	case OpcodeInfo::Asl_S1S2D:	// 00001100 00011110 010SsssD
		{
			const TWord sss = oi->getFieldValue(OpcodeInfo::Field_sss, op);
			const bool abDst = oi->getFieldValue(OpcodeInfo::Field_D, op);
			const bool abSrc = oi->getFieldValue(OpcodeInfo::Field_S, op);

			const TWord shiftAmount = decode_sss_read<TWord>( sss );

			alu_asl( abDst, abSrc, shiftAmount );			
		}
		return true;
	// Arithmetic Shift Accumulator Right
	case OpcodeInfo::Asr_ii:	// 00001100 00011100 SiiiiiiD
		{
			const TWord shiftAmount	= oi->getFieldValue(OpcodeInfo::Field_iiiiii, op);

			const bool abDst		= oi->getFieldValue(OpcodeInfo::Field_D, op);
			const bool abSrc		= oi->getFieldValue(OpcodeInfo::Field_S, op);

			alu_asr( abDst, abSrc, shiftAmount );
		}
		return true;
	case OpcodeInfo::Asr_S1S2D:
		{
			const TWord sss = oi->getFieldValue(OpcodeInfo::Field_sss, op);
			const bool abDst = oi->getFieldValue(OpcodeInfo::Field_D, op);
			const bool abSrc = oi->getFieldValue(OpcodeInfo::Field_S, op);

			const TWord shiftAmount = decode_sss_read<TWord>( sss );

			alu_asr( abDst, abSrc, shiftAmount );			
		}
		return true;		
	case OpcodeInfo::Lsl_ii:				// Logical Shift Left		000011000001111010iiiiiD
		{
            const auto shiftAmount = oi->getFieldValue(OpcodeInfo::Field_iiiii, op);
            const auto abDst = oi->getFieldValue(OpcodeInfo::Field_D, op);

            alu_lsl(abDst, shiftAmount);
        }
		return true;
	case OpcodeInfo::Lsl_SD:
		LOG_ERR_NOTIMPLEMENTED("LSL");
		return true;
	case OpcodeInfo::Lsr_ii:				// Logical Shift Right		000011000001111011iiiiiD
		{
            const auto shiftAmount = oi->getFieldValue(OpcodeInfo::Field_iiiii, op);
            const auto abDst = oi->getFieldValue(OpcodeInfo::Field_D, op);

            alu_lsr(abDst, shiftAmount);
        }
		return true;
	case OpcodeInfo::Lsr_SD:
		LOG_ERR_NOTIMPLEMENTED("LSR");
		return true;
	case OpcodeInfo::Eor_xx:				// Logical Exclusive OR
	case OpcodeInfo::Eor_xxxx:
		LOG_ERR_NOTIMPLEMENTED("EOR");
		return true;
	case OpcodeInfo::Extract_S1S2:			// Extract Bit Field
	case OpcodeInfo::Extract_CoS2:
		LOG_ERR_NOTIMPLEMENTED("EXTRACT");
		return true;
	case OpcodeInfo::Extractu_S1S2:			// Extract Unsigned Bit Field
	case OpcodeInfo::Extractu_CoS2:
		LOG_ERR_NOTIMPLEMENTED("EXTRACTU");
		return true;
	case OpcodeInfo::Insert_S1S2:			// Insert Bit Field
	case OpcodeInfo::Insert_CoS2:
		LOG_ERR_NOTIMPLEMENTED("INSERT");
		return true;
	case OpcodeInfo::Merge:					// Merge Two Half Words
		LOG_ERR_NOTIMPLEMENTED("MERGE");
		return true;
	case OpcodeInfo::Or_xx:					// Logical Inclusive OR
	case OpcodeInfo::Or_xxxx:
		LOG_ERR_NOTIMPLEMENTED("OR");
		return true;
	case OpcodeInfo::Ori:					// OR Immediate With Control Register - 00000000iiiiiiii111110EE
		{
			const TWord iiiiiiii = oi->getFieldValue(OpcodeInfo::Field_iiiiiiii, op);
			const TWord ee = oi->getFieldValue(OpcodeInfo::Field_EE, op);

			switch( ee )
			{
			case 0:	mr ( TReg8( mr().var | iiiiiiii) );	break;
			case 1:	ccr( TReg8(ccr().var | iiiiiiii) );	break;
			case 2:	com( TReg8(com().var | iiiiiiii) );	break;
			case 3:	eom( TReg8(eom().var | iiiiiiii) );	break;
			}
		}
		return true;
	default:
		return false;
	}
}

// _____________________________________________________________________________
// decode_cccc
//
bool DSP::decode_cccc( TWord cccc ) const
{
#define			SRT_C			sr_val(SRB_C)			// carry
#define 		SRT_V			sr_val(SRB_V)			// overflow
#define 		SRT_Z			sr_val(SRB_Z)			// zero
#define 		SRT_N			sr_val(SRB_N)			// negative
#define 		SRT_U			sr_val(SRB_U)			// unnormalized
#define 		SRT_E			sr_val(SRB_E)			// extension
#define 		SRT_L			sr_val(SRB_L)			// limit
//#define 		SRT_S			sr_val(SRB_S)			// scaling

	switch( cccc )
	{
	case CCCC_CarrySet:			return SRT_C!=0;								// CC(LO)		Carry Set	(lower)
	case CCCC_CarryClear:		return !SRT_C;									// CC(HS)		Carry Clear (higher or same)

	case CCCC_ExtensionSet:		return SRT_E!=0;								// ES			Extension set
	case CCCC_ExtensionClear:	return !SRT_E;									// EC			Extension clear

	case CCCC_Equal:			return SRT_Z!=0;								// EQ			Equal
	case CCCC_NotEqual:			return !SRT_Z;									// NE			Not Equal

	case CCCC_LimitSet:			return SRT_L!=0;								// LS			Limit set
	case CCCC_LimitClear:		return !SRT_L;									// LC			Limit clear

	case CCCC_Minus:			return SRT_N!=0;								// MI			Minus
	case CCCC_Plus:				return !SRT_N;									// PL			Plus

	case CCCC_GreaterEqual:		return SRT_N == SRT_V;							// GE			Greater than or equal
	case CCCC_LessThan:			return SRT_N != SRT_V;							// LT			Less than

	case CCCC_Normalized:		return (SRT_Z + ((!SRT_U) | (!SRT_E))) == 1;	// NR			Normalized
	case CCCC_NotNormalized:	return (SRT_Z + ((!SRT_U) | !SRT_E)) == 0;		// NN			Not normalized

	case CCCC_GreaterThan:		return (SRT_Z + (SRT_N != SRT_V)) == 0;			// GT			Greater than
	case CCCC_LessEqual:		return (SRT_Z + (SRT_N != SRT_V)) == 1;			// LE			Less than or equal
	}
	assert( 0 && "unreachable" );
	return false;
}

	void DSP::sr_debug(char* _dst) const
	{
		_dst[8] = 0;
		_dst[7] = sr_test(SR_C) ? 'C' : 'c';
		_dst[6] = sr_test(SR_V) ? 'V' : 'v';
		_dst[5] = sr_test(SR_Z) ? 'Z' : 'z';
		_dst[4] = sr_test(SR_N) ? 'N' : 'n';
		_dst[3] = sr_test(SR_U) ? 'U' : 'u';
		_dst[2] = sr_test(SR_E) ? 'E' : 'e';
		_dst[1] = sr_test(SR_L) ? 'L' : 'l';
		_dst[0] = sr_test(SR_S) ? 'S' : 's';
	}

	// _____________________________________________________________________________
// dumpCCCC
//
void DSP::dumpCCCC() const
{
	LOG( "CCCC Carry Clear        " << decode_cccc(0x0) );
	LOG( "CCCC >=                 " << decode_cccc(0x1) );
	LOG( "CCCC !=                 " << decode_cccc(0x2) );
	LOG( "CCCC Plus               " << decode_cccc(0x3) );
	LOG( "CCCC Not normalized     " << decode_cccc(0x4) );
	LOG( "CCCC Extension clear    " << decode_cccc(0x5) );
	LOG( "CCCC Limit clear        " << decode_cccc(0x6) );
	LOG( "CCCC >                  " << decode_cccc(0x7) );
	LOG( "CCCC Carry Set (lower)  " << decode_cccc(0x8) );
	LOG( "CCCC <                  " << decode_cccc(0x9) );
	LOG( "CCCC ==                 " << decode_cccc(0xa) );
	LOG( "CCCC Minus              " << decode_cccc(0xb) );
	LOG( "CCCC Normalized         " << decode_cccc(0xc) );
	LOG( "CCCC Extension Set      " << decode_cccc(0xd) );
	LOG( "CCCC Limit Set          " << decode_cccc(0xe) );
	LOG( "CCCC <=                 " << decode_cccc(0xf) );
}
// _____________________________________________________________________________
// exec_do
//
bool DSP::exec_do( TReg24 _loopcount, TWord _addr )
{
//	LOG( "DO BEGIN: " << (int)sc.var << ", loop flag = " << sr_test(SR_LF) );

	if( !_loopcount.var )
	{
		if( sr_test( SR_SC ) )
			_loopcount.var = 65536;
		else
		{
			setPC(_addr+1);
			return true;
		}
	}

	ssh(reg.la);
	ssl(reg.lc);

	LOGSC("DO #1");

	reg.la.var = _addr;
	reg.lc = _loopcount;

	pushPCSR();

	LOGSC( "DO #2");

	sr_set( SR_LF );

	return true;
}
// _____________________________________________________________________________
// exec_do_end
//
bool DSP::exec_do_end()
{
	// restore PC to point to the next instruction after the last instruction of the loop
	setPC(reg.la.var+1);

	// restore previous loop flag
	sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

	// decrement SP twice, restoring old loop settings
	decSP();
	LOGSC("DO end#1");

	reg.lc = ssl();
	reg.la = ssh();

	LOGSC("DO end#2");
//	LOG( "DO END: loop flag = " << sr_test(SR_LF) << " sc=" << (int)sc.var << " lc:" << std::hex << lc.var << " la:" << std::hex << la.var );

	return true;
}

// _____________________________________________________________________________
// alu_and
//
void DSP::alu_and( bool ab, TWord _val )
{
	TReg56& d = ab ? reg.b : reg.a;

	d.var &= (TInt64(_val)<<24);

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

	const TInt64 d64 = d.signextend<TInt64>();

	const TInt64 res = d64 + _val.signextend<TInt64>();

	d.var = res;
	d.doMasking();

	// S L E U N Z V C

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_v_update(res,d);
	sr_l_update_by_v();
	sr_c_update_arithmetic(old, d);
//	sr_c_update_logical(d);

//	dumpCCCC();
}

// _____________________________________________________________________________
// alu_cmp
//
void DSP::alu_cmp( bool ab, const TReg56& _val, bool _magnitude )
{
	TReg56& d = ab ? reg.b : reg.a;

	const TReg56 oldD = d;

	TInt64 d64 = d.signextend<TInt64>();

	TInt64 val = _val.signextend<TInt64>();

	if( _magnitude )
	{
		d64 = d64 < 0 ? -d64 : d64;
		val = val < 0 ? -val : val;
	}

	const TInt64 res = d64 - val;

	d.var = res;
	d.doMasking();

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_v_update(res,d);
	sr_l_update_by_v();
	sr_c_update_arithmetic(oldD,d);

	d = oldD;
}
// _____________________________________________________________________________
// alu_sub
//
void DSP::alu_sub( bool ab, const TReg56& _val )
{
	TReg56& d = ab ? reg.b : reg.a;

	const TReg56 old = d;

	const TInt64 d64 = d.signextend<TInt64>();

	const TInt64 res = d64 - _val.signextend<TInt64>();

	d.var = res;
	d.doMasking();

	// S L E U N Z V C

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_v_update(res,d);
	sr_l_update_by_v();
	sr_c_update_arithmetic( old, d );
//	sr_c_update_logical( d );
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
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_clear( SR_V );
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

	// S L E U N Z V C

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_clear( SR_V );
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
	TReg56&			d = ab ? reg.b : reg.a;
	const TReg56&	s = ab ? reg.a : reg.b;

	const TReg56 old = d;
	const TInt64 res = (d.signextend<TInt64>() >> 1) + s.signextend<TInt64>();
	d.var = res;
	d.doMasking();

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_v_update(res, d);
	sr_l_update_by_v();
	sr_c_update_arithmetic(old,d);
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
	sr_n_update_arithmetic(d);
	sr_z_update(d);
	sr_v_update(res, d);
	sr_l_update_by_v();
	sr_c_update_arithmetic(old,d);
}

inline void DSP::alu_clr(bool ab)
{
	auto& dst = ab ? reg.b : reg.a;
	dst.var = 0;

	sr_clear( SR_E | SR_N | SR_V );
	sr_set( SR_U | SR_Z );
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

	// TODO: revisit signed fraction multiplies
	auto res = s1 * s2;

	if( sr_test(SR_S0) )
	{
	}
	else if( sr_test(SR_S1) )
		res <<= 2;
	else
		res <<= 1;

	if( _negate )
		res = -res;

	TReg56& d = ab ? reg.b : reg.a;

	const TReg56 old = d;

	if( _accumulate )
		res += d.signextend<int64_t>();

	d.var = res & 0x00ffffffffffffff;

	// Update SR
	sr_s_update();
	sr_clear( SR_E );	// I don't know how this should happen because both operands are only 24 bits, the doc says "changed by standard definition" which should be okay here
	sr_u_update( d );
	sr_n_update_arithmetic( d );
	sr_z_update( d );
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
	if( sr_test(SR_S0) )
	{
	}
	else if( sr_test(SR_S1) )
		res <<= 2;
	else
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
	sr_n_update_arithmetic( d );
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
	if( sr_test(SR_S0) )
	{
	}
	else if( sr_test(SR_S1) )
		res <<= 2;
	else
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
	sr_n_update_arithmetic( d );
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
	if( sr_test(SR_S0) )
	{
	}
	else if( sr_test(SR_S1) )
		res <<= 2;
	else
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
	sr_n_update_arithmetic( d );
	sr_z_update( d );
	sr_v_update(res,d);

	sr_l_update_by_v();
}

// _____________________________________________________________________________
// alu_rnd
//
void DSP::alu_rnd( TReg56& _alu )
{
// 	if( sr_test( SR_S0 ) || sr_test( SR_S1 ) )
// 		LOG_ERR_NOTIMPLEMENTED( "scaling modes" );

	const int lsb = int(_alu.var & 0x000000ffffff);

	if( lsb > 0x800000 )
	{
		_alu.var += 0x1000000;
	}
	else if( !sr_test(SR_RM) && lsb == 0x800000 && (_alu.var & 0x000001000000) != 0 )
	{
		_alu.var += 0x1000000;
	}
	_alu.var &= 0x00ffffffff000000;	// TODO: wrong?
}

// _____________________________________________________________________________
// readReg
//
bool DSP::readReg( EReg _reg, TReg24& _res ) const
{
	switch( _reg )
	{
	case Reg_N0:	_res = reg.n[0];	break;
	case Reg_N1:	_res = reg.n[1];	break;
	case Reg_N2:	_res = reg.n[2];	break;
	case Reg_N3:	_res = reg.n[3];	break;
	case Reg_N4:	_res = reg.n[4];	break;
	case Reg_N5:	_res = reg.n[5];	break;
	case Reg_N6:	_res = reg.n[6];	break;
	case Reg_N7:	_res = reg.n[7];	break;

	case Reg_R0:	_res = reg.r[0];	break;
	case Reg_R1:	_res = reg.r[1];	break;
	case Reg_R2:	_res = reg.r[2];	break;
	case Reg_R3:	_res = reg.r[3];	break;
	case Reg_R4:	_res = reg.r[4];	break;
	case Reg_R5:	_res = reg.r[5];	break;
	case Reg_R6:	_res = reg.r[6];	break;
	case Reg_R7:	_res = reg.r[7];	break;

	case Reg_M0:	_res = reg.m[0];	break;
	case Reg_M1:	_res = reg.m[1];	break;
	case Reg_M2:	_res = reg.m[2];	break;
	case Reg_M3:	_res = reg.m[3];	break;
	case Reg_M4:	_res = reg.m[4];	break;
	case Reg_M5:	_res = reg.m[5];	break;
	case Reg_M6:	_res = reg.m[6];	break;
	case Reg_M7:	_res = reg.m[7];	break;

	case Reg_A0:	_res = a0();	break;
	case Reg_A1:	_res = a1();	break;
	case Reg_B0:	_res = b0();	break;
	case Reg_B1:	_res = b1();	break;

	case Reg_X0:	_res = x0();	break;
	case Reg_X1:	_res = x1();	break;

	case Reg_Y0:	_res = y0();	break;
	case Reg_Y1:	_res = y1();	break;

	case Reg_PC:	_res = reg.pc;		break;
	case Reg_SR:	_res = reg.sr;		break;
	case Reg_OMR:	_res = reg.omr;		break;
	case Reg_SP:	_res = reg.sp;		break;

	case Reg_LA:	_res = reg.la;		break;
	case Reg_LC:	_res = reg.lc;		break;

	case Reg_ICTR:	_res = reg.ictr;	break;

	default:
		return false;
	}

	return true;
}
// _____________________________________________________________________________
// readReg
//
bool DSP::readReg( EReg _reg, TReg56& _res ) const
{
	switch( _reg )
	{
	case Reg_A:		_res = reg.a;	return true;
	case Reg_B:		_res = reg.b;	return true;
	}
	return false;
}
// _____________________________________________________________________________
// readReg
//
bool DSP::readReg( EReg _reg, TReg8& _res ) const
{
	switch( _reg )
	{
	case Reg_A2:	_res = a2();	return true;
	case Reg_B2:	_res = b2();	return true;
	}
	return false;
}
// _____________________________________________________________________________
// readReg
//
bool DSP::readReg( EReg _reg, TReg48& _res ) const
{
	switch( _reg )
	{
	case Reg_X:		_res = reg.x;	return true;
	case Reg_Y:		_res = reg.y;	return true;
	}
	return false;
}
// _____________________________________________________________________________
// readReg
//
bool DSP::readReg( EReg _reg, TReg5& _res ) const
{
	if( _reg == Reg_SC )
	{
		_res = reg.sc;
		return true;
	}
	return false;
}

// _____________________________________________________________________________
// readRegToInt
//
bool DSP::readRegToInt( EReg _reg, int64_t& _dst )
{
	switch( g_regBitCount[_reg] )
	{
	case 56:
		{
			TReg56 dst;
			if( !readReg(_reg, dst) )
				return false;
			_dst = dst.var;
		};
		break;
	case 48:
		{
			TReg48 dst;
			if( !readReg(_reg, dst) )
				return false;
			_dst = dst.var;
		};
		break;
	case 24:
		{
			TReg24 dst;
			if( !readReg(_reg, dst) )
				return false;
			_dst = dst.var;
		};
		break;
	case 8:
		{
			TReg8 dst;
			if( !readReg(_reg, dst) )
				return false;
			_dst = dst.var;
		};
		break;
	case 5:
		{
			TReg5 dst;
			if( !readReg(_reg, dst) )
				return false;
			_dst = dst.var;
		};
		break;
	default:
		return false;
	}
	return true;
}
// _____________________________________________________________________________
// writeReg
//
bool DSP::writeReg( EReg _reg, const TReg24& _res )
{
	assert( (_res.var & 0xff000000) == 0 );
		
	switch( _reg )
	{
	case Reg_N0:	reg.n[0] = _res;	break;
	case Reg_N1:	reg.n[1] = _res;	break;
	case Reg_N2:	reg.n[2] = _res;	break;
	case Reg_N3:	reg.n[3] = _res;	break;
	case Reg_N4:	reg.n[4] = _res;	break;
	case Reg_N5:	reg.n[5] = _res;	break;
	case Reg_N6:	reg.n[6] = _res;	break;
	case Reg_N7:	reg.n[7] = _res;	break;
						
	case Reg_R0:	reg.r[0] = _res;	break;
	case Reg_R1:	reg.r[1] = _res;	break;
	case Reg_R2:	reg.r[2] = _res;	break;
	case Reg_R3:	reg.r[3] = _res;	break;
	case Reg_R4:	reg.r[4] = _res;	break;
	case Reg_R5:	reg.r[5] = _res;	break;
	case Reg_R6:	reg.r[6] = _res;	break;
	case Reg_R7:	reg.r[7] = _res;	break;
						
	case Reg_M0:	reg.m[0] = _res;	break;
	case Reg_M1:	reg.m[1] = _res;	break;
	case Reg_M2:	reg.m[2] = _res;	break;
	case Reg_M3:	reg.m[3] = _res;	break;
	case Reg_M4:	reg.m[4] = _res;	break;
	case Reg_M5:	reg.m[5] = _res;	break;
	case Reg_M6:	reg.m[6] = _res;	break;
	case Reg_M7:	reg.m[7] = _res;	break;
						
	case Reg_A0:	a0(_res);		break;
	case Reg_A1:	a1(_res);		break;
	case Reg_B0:	b0(_res);		break;
	case Reg_B1:	b1(_res);		break;
						
	case Reg_X0:	x0(_res);		break;
	case Reg_X1:	x1(_res);		break;
						
	case Reg_Y0:	y0(_res);		break;
	case Reg_Y1:	y1(_res);		break;

	case Reg_PC:	setPC(_res);	break;

	default:
		assert( 0 && "unknown register" );
		return false;
	}

	return true;
}

// _____________________________________________________________________________
// writeReg
//
bool DSP::writeReg( EReg _reg, const TReg56& _val )
{
	switch( _reg )
	{
	case Reg_A:		reg.a = _val;		return true;
	case Reg_B:		reg.b = _val;		return true;
	}
	assert( 0 && "unknown register" );
	return false;
}

// _____________________________________________________________________________
// readDebugRegs
//
void DSP::readDebugRegs( dsp56k::SRegs& _regs ) const
{
	readReg( Reg_X, _regs.x);
	readReg( Reg_Y, _regs.y);

	readReg( Reg_A, _regs.a);
	readReg( Reg_B, _regs.b);

	readReg( Reg_X0, _regs.x0);		
	readReg( Reg_X1, _regs.x1);		

	readReg( Reg_Y0, _regs.y0);		
	readReg( Reg_Y1, _regs.y1);		

	readReg( Reg_A0, _regs.a0);		
	readReg( Reg_A1, _regs.a1);		
	readReg( Reg_A2, _regs.a2);		

	readReg( Reg_B0, _regs.b0);		
	readReg( Reg_B1, _regs.b1);		
	readReg( Reg_B2, _regs.b2);		

	readReg( Reg_PC, _regs.pc);		
	readReg( Reg_SR, _regs.sr);		
	readReg( Reg_OMR, _regs.omr);	

	readReg( Reg_LA, _regs.la);		
	readReg( Reg_LC, _regs.lc);		

	readReg( Reg_SSH, _regs.ssh);	
	readReg( Reg_SSL, _regs.ssl);	
	readReg( Reg_SP, _regs.sp);		

	readReg( Reg_EP, _regs.ep);		
	readReg( Reg_SZ, _regs.sz);		
	readReg( Reg_SC, _regs.sc);		
	readReg( Reg_VBA, _regs.vba);	

	readReg( Reg_IPRC, _regs.iprc);	
	readReg( Reg_IPRP, _regs.iprp);	
	readReg( Reg_BCR, _regs.bcr);	
	readReg( Reg_DCR, _regs.dcr);	

	readReg( Reg_AAR0, _regs.aar0);	
	readReg( Reg_AAR1, _regs.aar1);	
	readReg( Reg_AAR2, _regs.aar2);	
	readReg( Reg_AAR3, _regs.aar3);	

	readReg( Reg_R0, _regs.r0);		
	readReg( Reg_R1, _regs.r1);		
	readReg( Reg_R2, _regs.r2);		
	readReg( Reg_R3, _regs.r3);		
	readReg( Reg_R4, _regs.r4);		
	readReg( Reg_R5, _regs.r5);		
	readReg( Reg_R6, _regs.r6);		
	readReg( Reg_R7, _regs.r7);		

	readReg( Reg_N0, _regs.n0);		
	readReg( Reg_N1, _regs.n1);		
	readReg( Reg_N2, _regs.n2);		
	readReg( Reg_N3, _regs.n3);		
	readReg( Reg_N4, _regs.n4);		
	readReg( Reg_N5, _regs.n5);		
	readReg( Reg_N6, _regs.n6);		
	readReg( Reg_N7, _regs.n7);		

	readReg( Reg_M0, _regs.m0);		
	readReg( Reg_M1, _regs.m1);		
	readReg( Reg_M2, _regs.m2);		
	readReg( Reg_M3, _regs.m3);		
	readReg( Reg_M4, _regs.m4);		
	readReg( Reg_M5, _regs.m5);		
	readReg( Reg_M6, _regs.m6);		
	readReg( Reg_M7, _regs.m7);		

	readReg( Reg_HIT, _regs.hit);		
	readReg( Reg_MISS, _regs.miss);		
	readReg( Reg_REPLACE, _regs.replace);	
	readReg( Reg_CYC, _regs.cyc);		
	readReg( Reg_ICTR, _regs.ictr);		
	readReg( Reg_CNT1, _regs.cnt1);		
	readReg( Reg_CNT2, _regs.cnt2);		
	readReg( Reg_CNT3, _regs.cnt3);		
	readReg( Reg_CNT4, _regs.cnt4);
}

// _____________________________________________________________________________
// getASM
//
const char* DSP::getASM()
{
#ifdef _DEBUG
	unsigned long ops[3];
	ops[0] = memRead( MemArea_P, reg.pc.var );
	ops[1] = memRead( MemArea_P, reg.pc.var+1 );
	ops[2] = memRead( MemArea_P, reg.pc.var+2 );
	disassemble( m_asm, ops, reg.sr.var, reg.omr.var );
#endif
	return m_asm;
}

// _____________________________________________________________________________
// memWrite
//
bool DSP::memWrite( EMemArea _area, TWord _offset, TWord _value )
{
	return mem.set( _area, _offset, _value );
}

// _____________________________________________________________________________
// memRead
//
dsp56k::TWord DSP::memRead( EMemArea _area, TWord _offset ) const
{
	// app may access the instruction cache on the DSP 56362? not sure about this, not clear for me in the docs

// 	if( _area == MemArea_P && sr_test(SR_CE) )
// 	{
// 		const bool ms = (reg.omr.var & OMR_MS) != 0;
// 
// 		if( !ms )
// 		{
// 			if( _offset >= 0x000800 && _offset < 0x000c00 )
// 			{
// 				return cache.readMemory( _offset - 0x000800 );
// 			}
// 		}
// 		else
// 		{
// 			if( _offset >= 0x001000 && _offset < 0x001400 )
// 				return cache.readMemory( _offset - 0x001000 );
// 		}
// 	}

		return mem.get( _area, _offset );
}

// _____________________________________________________________________________
// alu_abs
//
void DSP::alu_abs( bool ab )
{
	TReg56& d = ab ? reg.b : reg.a;

	TInt64 d64 = d.signextend<TInt64>();

	d64 = d64 < 0 ? -d64 : d64;

	d.var = d64 & 0xffffffffffffff;

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_logical(d);
	sr_z_update(d);
//	sr_v_update(d);
	sr_l_update_by_v();
}

void DSP::alu_tfr(const bool ab, const TReg56& src)
{
	auto& d = ab ? reg.b : reg.a;
	d = src;
	sr_s_update();
	sr_v_update(src.var, d);
}

void DSP::alu_tst(bool ab)
{
	const bool c = sr_test(SR_C);
	alu_cmp(ab, TReg56(0), false);

	sr_clear(SR_V);		// "always cleared"
	sr_toggle(SR_C,c);	// "unchanged by the instruction" so reset to previous state
}

void DSP::alu_neg(bool ab)
{
	TReg56& d = ab ? reg.b : reg.a;

	TInt64 d64 = d.signextend<TInt64>();
	d64 = -d64;
	
	d.var = d64 & 0x00ffffffffffffff;

	sr_s_update();
	sr_e_update(d);
	sr_u_update(d);
	sr_n_update_logical(d);
	sr_z_update(d);
//	TODO: how to update v? test in sim		sr_v_update(d);
	sr_l_update_by_v();
}

// _____________________________________________________________________________
// save
//
bool DSP::save( FILE* _file ) const
{
	fwrite( &reg, sizeof(reg), 1, _file );
	fwrite( &repRunning, 1, 1, _file );
	fwrite( &tempLCforRep, 1, 1, _file );
	fwrite( &pcLastExec, 1, 1, _file );
	fwrite( m_asm, sizeof(m_asm), 1, _file );
	fwrite( &cache, sizeof(cache), 1, _file );

	return true;
}

// _____________________________________________________________________________
// load
//
bool DSP::load( FILE* _file )
{
	fread( &reg, sizeof(reg), 1, _file );
	fread( &repRunning, 1, 1, _file );
	fread( &tempLCforRep, 1, 1, _file );
	fread( &pcLastExec, 1, 1, _file );
	fread( m_asm, sizeof(m_asm), 1, _file );
	fread( &cache, sizeof(cache), 1, _file );

	return true;
}

}
