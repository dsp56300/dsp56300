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

				signed __int64 regVal = 0;
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

	bool DSP::exec_parallel(TWord op)
	{
		const auto* oi = m_opcodes.findParallelMoveOpcodeInfo(op & 0xffff00);

		switch (oi->getInstruction())
		{
		case OpcodeInfo::Ifcc:
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

			res |= exec_parallel_alu(op);//(exec_arithmetic_parallel(dbmfop,dbmf,op) || exec_logical_parallel(dbmfop,dbmf,op));

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

	bool DSP::exec_parallel_alu_multiply(TWord op)
	{
		const auto round = op & 0x1;			// TODO: rounding
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
		case OpcodeInfo::Ifcc:
			LOG_ERR_NOTIMPLEMENTED("IFcc");
			return true;
		case OpcodeInfo::Ifcc_U: 
			LOG_ERR_NOTIMPLEMENTED("IFcc.u");
			return true;
		case OpcodeInfo::Move_Nop:		// NO parallel data move
			return true;
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
					if( mmmrrr == 0x34 )
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
		const auto dbmfop = op;
		const auto dbmf = op & 0xffff00;
		op &= 0xff;

		if( exec_operand_8bits(dbmf,op) )
		{
		}
		else if( exec_move(dbmfop,dbmf,op) )
		{
		}
		else if( exec_pcu(dbmfop,dbmf,op) )
		{
		}
		else if( exec_bitmanip(dbmfop,dbmf,op) )
		{
		}
		else if( exec_logical_nonparallel(dbmfop,dbmf,op) )
		{
		}

		// ADD #xx, D		- 00000001 01iiiiii 1000d000
		else if( (dbmf&0xffc000) == 0x014000 && (op&0xf7) == 0x80 )
		{
			const TReg56 iiiiii = TReg56(TInt64((dbmfop&0x003f00) >> 8) << 24);

			alu_add( (op&0x08) != 0, iiiiii );
		}

		// ADD #xxxx, D		- 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 0 1 1 0 0 d 0 0 0
		else if( (dbmf&0xffff00) == 0x014000 && (op&0xf7) == 0xc0 )
		{
			TReg56 r56;
			convert( r56, TReg24(fetchPC()) );

			alu_add( (op&0x08) != 0, r56 );
		}

		// Logical AND

		// AND #xx, D		- 0 0 0 0 0 0 0 1 0 1 i i i i i i 1 0 0 0 d 1 1 0
		else if( (dbmf&0xffc000) == 0x014000 && (op&0xf7) == 0x86 )
		{
			TReg56& d = bittest( op, 3 ) ? reg.b : reg.a;
			const TWord xxxx = (dbmfop&0x003f00)>>8;

			alu_and( bittest( op, 3 ), xxxx );
		}

		// AND #xxxx, D		- 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 0 1 1 0 0 d 1 1 0
		else if( (dbmf&0xffff00) == 0x014000 && (op&0xf7) == 0xc6 )
		{
			const TWord xxxx = fetchPC();

			alu_and( bittest( op, 3 ), xxxx );
		}

		// Arithmetic Shift Accumulator Left

		// ASL, #ii, S2, D	- 00001100 00011101 SiiiiiiD
		else if( (dbmf&0xffff00) == 0x0c1d00 )
		{
			const TWord shiftAmount	= ((dbmfop&0x00007e)>>1);

			const bool abDst		= bittest(dbmfop,0);
			const bool abSrc		= bittest(dbmfop,7);

			alu_asl( abDst, abSrc, shiftAmount );
		}

		// ASL S1,S2,D	- 00001100 00011110 010SsssD - arithmetic shift accumulator left
		else if( (dbmf&0xffff00) == 0x0c1e00 && (op&0xe0) == 0x40 )
		{
			const TWord sss = (dbmfop>>1)&0x7;
			const bool abDst = bittest(dbmfop,0);
			const bool abSrc = bittest(dbmfop,4);

			const TWord shiftAmount = decode_sss_read<TWord>( sss );

			alu_asl( abDst, abSrc, shiftAmount );
		}

		// Arithmetic Shift Accumulator Right

		// ASR, #ii, S2, D	- 00001100 00011100 SiiiiiiD
		else if( (dbmf&0xffff00) == 0x0c1c00 )
		{
			TWord shiftAmount = ((dbmfop&0x00007e)>>1);

			const bool abDst = bittest( dbmfop, 0 );
			const bool abSrc = bittest( dbmfop, 7 );

			alu_asr( abDst, abSrc, shiftAmount );
		}

		// ASR S1,S2,D	- 00001100 00011110 011SsssD
		else if( (dbmf&0xffff00) == 0x0c1e00 && (op&0xe0) == 0x60 )
		{
			const TWord sss = (dbmfop>>1)&0x7;
			const bool abDst = bittest(dbmfop,0);
			const bool abSrc = bittest(dbmfop,4);

			const TWord shiftAmount = decode_sss_read<TWord>( sss );

			alu_asr( abDst, abSrc, shiftAmount );
		}

		// Branch if Bit Clear

		// BRCLR #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0c8000 && (op&0xa0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRCLR");
		}

		// BRCLR #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0c8000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRCLR");
		}

		// BRCLR #n,[X or Y]:pp,xxxx - 0 0 0 0 1 1 0 0 1 1 p p p p p p 0 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0cc000 && (op&0xa0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRCLR");
		}

		// BRCLR #n,[X or Y]:qq,xxxx - 00000100 10qqqqqq 0S0bbbbb
		else if( (dbmf&0xffc000) == 0x048000 && (op&0xa0) == 0x00 )
		{
			const TWord bit		= dbmfop&0x00001f;
			const TWord qqqqqq	= (dbmfop&0x003f00)>>8;
			const EMemArea S	= bittest( dbmfop, 6 ) ? MemArea_Y : MemArea_X;

			const TWord ea = 0xffff80 + qqqqqq;

			const int displacement = signextend<int,24>( fetchPC() );

			if( !bittest( memRead( S, ea ), bit ) )
			{
				reg.pc.var += (displacement-2);
			}
		}

		// BRCLR #n,S,xxxx - 0 0 0 0 1 1 0 0 1 1 D D D D D D 1 0 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0cc000 && (op&0xe0) == 0x80 )
		{
			const TWord bit		= dbmfop&0x00001f;
			const TWord dddddd	= (dbmfop&0x003f00)>>8;

			const TReg24 tst = decode_dddddd_read( dddddd );

			const int displacement = signextend<int,24>( fetchPC() );

			if( !bittest( tst, bit ) )
			{
				reg.pc.var += (displacement-2);
			}
		}

		// Exit Current DO Loop Conditionally

		// BRKcc - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 1 C C C C
		else if( (dbmf&0xffff00) == 0x000200 && (op&0xf0) == 0x10 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRKcc");
		}

		// Branch if Bit Set

		// BRSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0c8000 && (op&0xa0) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRSET");
		}

		// BRSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0c8000 && (op&0xa0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRSET");
		}

		// BRSET #n,[X or Y]:pp,xxxx - 
		else if( (dbmf&0xffc000) == 0x0cc000 && (op&0xa0) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRSET");
		}

		// BRSET #n,[X or Y]:qq,xxxx - 00000100 10qqqqqq 0S1bbbbb
		else if( (dbmf&0xffc000) == 0x048000 && (op&0xa0) == 0x20 )
		{
			const TWord bit		= dbmfop&0x00001f;
			const TWord qqqqqq	= (dbmfop&0x003f00)>>8;
			const EMemArea S	= bittest( dbmfop, 6 ) ? MemArea_Y : MemArea_X;

			const TWord ea = 0xffff80 + qqqqqq;

			const int displacement = signextend<int,24>( fetchPC() );

			if( bittest( memRead( S, ea ), bit ) )
			{
				reg.pc.var += (displacement-2);
			}
		}

		// BRSET #n,S,xxxx - 00001100 11DDDDDD 101bbbbb
		else if( (dbmf&0xffc000) == 0x0cc000 && (op&0xe0) == 0xa0 )
		{
			const TWord bit		= dbmfop&0x00001f;
			const TWord dddddd	= (dbmfop&0x003f00)>>8;

			TReg24 r = decode_dddddd_read( dddddd );

			const int displacement = signextend<int,24>( fetchPC() );

			if( bittest( r.var, bit ) )
			{
				reg.pc.var += (displacement-2);
			}
		}

		// Branch to Subroutine if Bit Clear

		// BSCLR #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 1 1 0 M M M R R R 0 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0d8000 && (op&0xa0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
		}

		// BSCLR #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 1 1 0 a a a a a a 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0d8000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
		}

		// BSCLR #n,[X or Y]:qq,xxxx - 0 0 0 0 0 1 0 0 1 0 q q q q q q 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x048000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
		}

		// BSCLR #n,[X or Y]:pp,xxxx - 0 0 0 0 1 1 0 1 1 1 p p p p p p 0 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0dc000 && (op&0xa0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
		}

		// BSCLR #n,S,xxxx - 0 0 0 0 1 1 0 1 1 1 D D D D D D 1 0 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0dc000 && (op&0xa0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
		}

		// Branch to Subroutine if Bit Set

		// BSSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 1 1 0 M M M R R R 0 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0d8000 && (op&0xa0) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSSET");
		}

		// BSSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 1 1 0 a a a a a a 1 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0d8000 && (op&0xa0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSSET");
		}

		// BSSET #n,[X or Y]:pp,xxxx - 0 0 0 0 1 1 0 1 1 1 p p p p p p 0 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0dc000 && (op&0xa0) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSSET");
		}

		// BSSET #n,[X or Y]:qq,xxxx - 0 0 0 0 0 1 0 0 1 0 q q q q q q 1 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x048000 && (op&0xa0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSSET");
		}

		// BSSET #n,S,xxxx - 0 0 0 0 1 1 0 1 1 1 D D D D D D 1 0 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0dc000 && (op&0xe0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSSET");
		}

		// Count Leading Bits

		// CLB S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 S D
		else if( (dbmf&0xffff00) == 0x0c1e00 && (op&0xfc) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("CLB");
		}

		// Compare

		// CMP #xx, S2 - 00000001 01iiiiii 1000d101
		else if( (dbmf&0xffc000) == 0x014000 && (op&0xf7) == 0x85 )
		{
			const TWord iiiiii = (dbmfop&0x003f00)>>8;
			
			TReg56 r56;
			convert( r56, TReg24(iiiiii) );

			alu_cmp( bittest(op,3), r56, false );
		}

		// CMP #xxxx,S2 - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 0 1 1 0 0 d 1 0 1
		else if( (dbmf&0xffff00) == 0x014000 && (op&0xf7) == 0xc5 )
		{
			const TReg24 s( signextend<int,24>( fetchPC() ) );

			TReg56 r56;
			convert( r56, s );

			alu_cmp( bittest(op,3), r56, false );
		}

		// Compare Unsigned

		// CMPU S1, S2 - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 1 1 1 1 1 g g g d
		else if( (dbmf&0xffff00) == 0x0c1f00 && (op&0xf0) == 0xf0 )
		{
			LOG_ERR_NOTIMPLEMENTED("CMPU");
		}

		// Divide Iteration

		// DIV S,D - 0 0 0 0 0 0 0 1 1 0 0 0 0 0 0 0 0 1 J J d 0 0 0
		else if( (dbmf&0xffff00) == 0x018000 && (op&0xc7) == 0x40 )
		{
			// TODO: i'm not sure if this works as expected...

			const TWord jj = ((dbmfop&0x00003f)>>4);

			TReg56& d = bittest( dbmfop, 3 ) ? reg.b : reg.a;

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

		// Double-Precision Multiply-Accumulate With Right Shift

		// DMAC (±)S1,S2,D - 00000001 0010010s 1sdkQQQQ
		else if( (dbmf&0xfffe00) == 0x012400 && (op&0x80) == 0x80 )
		{
			const bool dstUnsigned	= bittest(dbmfop,8);
			const bool srcUnsigned	= bittest(dbmfop,6);
			const bool ab			= bittest(dbmfop,5);
			const bool negate		= bittest(dbmfop,4);

			const TWord qqqq		= dbmfop&0x00000f;

			TReg24 s1, s2;
			decode_QQQQ_read( s1, s2, qqqq );

			alu_dmac( ab, s1, s2, negate, srcUnsigned, dstUnsigned );
		}

		// Start Hardware Loop

		// DO [X or Y]:ea, expr - 0 0 0 0 0 1 1 0 0 1 M M M R R R 0 S 0 0 0 0 0 0
		else if( (dbmf&0xffc000) == 0x064000 && (op&0xbf) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("DO");
		}

		// DO [X or Y]:aa, expr - 0 0 0 0 0 1 1 0 0 0 a a a a a a 0 S 0 0 0 0 0 0
		else if( (dbmf&0xffc000) == 0x060000 && (op&0xbf) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("DO");
		}

		// DO #xxx, expr - 0 0 0 0 0 1 1 0 i i i i i i i i 1 0 0 0 h h h h
		else if( (dbmf&0xff0000) == 0x060000 && (op&0xf0) == 0x80 )
		{
			const TWord addr = fetchPC();
			TWord loopcount = ((dbmfop&0x00000f) << 8) | ((dbmfop&0x00ff00)>>8);

			exec_do( TReg24(loopcount), addr );
		}

		// DO S, expr - 00000110 11DDDDDD 00000000
		else if( (dbmf&0xffc000) == 0x06c000 && op == 0x00 )
		{
			const TWord addr = fetchPC();

			const TWord dddddd = ((dbmfop&0x003f00)>>8);

			const TReg24 loopcount = decode_dddddd_read( dddddd );

			exec_do( loopcount, addr );
		}

		// Start Infinite Loop

		// DO FOREVER - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1 1
		else if( (dbmf&0xffff00) == 0x000200 && op == 0x03 )
		{
			LOG_ERR_NOTIMPLEMENTED("DO FOREVER");
		}

		// Start PC-Relative Hardware Loop

		// DOR [X or Y]:ea,label - 0 0 0 0 0 1 1 0 0 1 M M M R R R 0 S 0 1 0 0 0 0
		else if( (dbmf&0xffc000) == 0x064000 && (op&0xbf) == 0x10 )
		{
			LOG_ERR_NOTIMPLEMENTED("DOR");
		}

		// DOR [X or Y]:aa,label - 0 0 0 0 0 1 1 0 0 0 a a a a a a 0 S 0 1 0 0 0 0
		else if( (dbmf&0xffc000) == 0x060000 && (op&0xbf) == 0x10 )
		{
			LOG_ERR_NOTIMPLEMENTED("DOR");
		}

		// DOR #xxx, label - 0 0 0 0 0 1 1 0 i i i i i i i i 1 0 0 1 h h h h
		else if( (dbmf&0xff0000) == 0x060000 && (op&0xf0) == 0x90 )
		{
			LOG_ERR_NOTIMPLEMENTED("DOR");
		}

		// DOR S, label - 00000110 11DDDDDD 00010000
		else if( (dbmf&0xffc000) == 0x06c000 && op == 0x10 )
		{
			const TWord dddddd	= (dbmfop&0x003f00)>>8;
			const TReg24 lc		= decode_dddddd_read( dddddd );
			
			const int displacement = signextend<int,24>(fetchPC());
			exec_do( lc, reg.pc.var + displacement - 2 );
		}

		// Start PC-Relative Infinite Loops

		// DOR FOREVER - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 1 0
		else if( (dbmf&0xffff00) == 0x000200 && op == 0x02 )
		{
			LOG_ERR_NOTIMPLEMENTED("DOR FOREVER");
		}

		// Load PC-Relative Address

		// LRA Rn,D - 0 0 0 0 0 1 0 0 1 1 0 0 0 R R R 0 0 0 d d d d d
		else if( (dbmf&0xfff800) == 0x04c000 && (op&0xe0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("LRA");
		}

		// LRA xxxx,D - 0 0 0 0 0 1 0 0 0 1 0 0 0 0 0 0 0 1 0 d d d d d
		else if( (dbmf&0xffff00) == 0x044000 && (op&0xe0) == 0x40 )
		{
			LOG_ERR_NOTIMPLEMENTED("LRA");
		}

		// Load Updated Address

		// LUA/LEA ea,D - 00000100 010MMRRR 000ddddd
		else if( (dbmf&0xffe000) == 0x044000 && (op&0xe0) == 0x00 )
		{
			const TWord mmrrr	= (dbmfop&0x001f00)>>8;
			const TWord ddddd	= (dbmfop&0x00001f);

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

		// LUA/LEA (Rn + aa),D - 00000100 00aaaRRR aaaadddd
		else if( (dbmf&0xffc000) == 0x040000 )
		{
			const TWord dddd	= (dbmfop&0x00000f);
			const TWord a		= ((dbmfop&0x003800)>>7) | ((dbmfop&0x0000f0)>>4);
			const TWord rrr		= (dbmfop&0x000700)>>8;

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

		// Signed Multiply Accumulate

		// MAC (±)S,#n,D - 00000001 000sssss 11QQdk10
		else if( (dbmf&0xffe000) == 0x010000 && (op&0xc3) == 0xc2 )
		{
			const TWord sssss	= (dbmfop&0x001f00)>>8;
			const TWord qq		= (dbmfop&0x000030)>>4;
			const bool	ab		= bittest(dbmfop,3);
			const bool	negate	= bittest(dbmfop,2);

			const TReg24 s1 = decode_QQ_read( qq );
			const TReg24 s2( decode_sssss(sssss) );

			alu_mac( ab, s1, s2, negate, false );
		}

		// Signed Multiply Accumulate With Immediate Operand

		// MACI (±)#xxxx,S,D - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 1 1 1 q q d k 1 0
		else if( (dbmf&0xffff00) == 0x014100 && (op&0xc3) == 0xc2 )
		{
			LOG_ERR_NOTIMPLEMENTED("MACI");
		}

		// Mixed Multiply Accumulate

		// MACsu (±)S1,S2,D - 0 0 0 0 0 0 0 1 0 0 1 0 0 1 1 0 1 s d k Q Q Q Q
		// MACuu (±)S1,S2,D
		else if( (dbmf&0xffff00) == 0x012600 && (op&0x80) == 0x80 )
		{
			const bool uu			= bittest(dbmfop,6);
			const bool ab			= bittest(dbmfop,5);
			const bool negate		= bittest(dbmfop,4);

			const TWord qqqq		= dbmfop&0x00000f;

			TReg24 s1, s2;
			decode_QQQQ_read( s1, s2, qqqq );

			alu_mac( ab, s1, s2, negate, uu );
		}

		// Signed Multiply Accumulate and Round

		// MACR (±)S,#n,D - 0 0 0 0 0 0 0 1 0 0 0 0 3 s s s 1 1 Q Q d k 1 1
		else if( (dbmf&0xfff000) == 0x010000 && (op&0xc3) == 0xc3 )
		{
			LOG_ERR_NOTIMPLEMENTED("MACR");
		}

		// Signed MAC and Round With Immediate Operand

		// MACRI (±)#xxxx,S,D - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 1 1 1 q q d k 1 1
		else if( (dbmf&0xffff00) == 0x014100 && (op&0xc3) == 0xc3 )
		{
			LOG_ERR_NOTIMPLEMENTED("MACRI");
		}

		// Signed Multiply

		// MPY (±)S,#n,D - 00000001 000sssss 11QQdk00
		else if( (dbmf&0xffe000) == 0x010000 && (op&0xc3) == 0xc0 )
		{
			const int sssss		= (dbmfop&0x001f00)>>8;
			const TWord QQ		= (dbmfop&0x000030)>>4;
			const bool ab		= bittest( dbmfop, 3 );
			const bool negate	= bittest( dbmfop, 2 );

			const TReg24 s1 = decode_QQ_read(QQ);

			const TReg24 s2 = TReg24( decode_sssss(sssss) );

			alu_mpy( ab, s1, s2, negate, false );
		}

		// Mixed Multiply

		// MPY su (±)S1,S2,D - 0 0 0 0 0 0 0 1 0 0 1 0 0 1 1 1 1 s d k Q Q Q Q
		// MPY uu (±)S1,S2,D
		else if( (dbmf&0xffff00) == 0x012700 && (op&0x80) == 0x80 )
		{
			const bool negate	= bittest(dbmfop,4);
			const bool ab		= bittest(dbmfop,5);
			const bool uu		= bittest(dbmfop,6);
			const TWord qqqq	= dbmfop&0x00000f;

			TReg24 s1, s2;
			decode_QQQQ_read( s1, s2, qqqq );

			alu_mpysuuu( ab, s1, s2, negate, false, uu );
		}

		// Signed Multiply With Immediate Operand

		// MPYI (±)#xxxx,S,D - 00000001 01000001 11qqdk00
		else if( (dbmf&0xffff00) == 0x014100 && (op&0xc3) == 0xc0 )
		{
			const TReg24 s		= TReg24(fetchPC());
			const TWord qq		= (dbmfop&0x000030)>>4;
			const bool	negate	= bittest(op,2);
			const bool	ab		= bittest(op,3);

			const TReg24 reg	= decode_qq_read(qq);

			alu_mpy( ab, reg, s, negate, false );
		}

		// Signed Multiply and Round

		// MPYR (±)S,#n,D - 0 0 0 0 0 0 0 1 0 0 0 s s s s s 1 1 Q Q d k 0 1
		else if( (dbmf&0xffe000) == 0x010000 && (op&0xc3) == 0xc1 )
		{
			LOG_ERR_NOTIMPLEMENTED("MPYR");
		}

		// Signed Multiply and Round With Immediate Operand

		// MPYRI (±)#xxxx,S,D - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 1 1 1 q q d k 0 1
		else if( (dbmf&0xffff00) == 0x014100 && (op&0xc3) == 0xc1 )
		{
			LOG_ERR_NOTIMPLEMENTED("MPYRI");
		}

		// Norm Accumulator Iterations

		// NORM Rn,D - 0 0 0 0 0 0 0 1 1 1 0 1 1 R R R 0 0 0 1 d 1 0 1
		else if( (dbmf&0xfff800) == 0x01d800 && (op&0xf7) == 0x15 )
		{
			LOG_ERR_NOTIMPLEMENTED("NORM");
		}

		// Fast Accumulator Normalization

		// NORMF S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 1 0 s s s D
		else if( (dbmf&0xffff00) == 0x0c1e00 && (op&0xf0) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("NORMF");
		}

		// AND Immediate With Control Register

		// AND(I) #xx,D		- 00000000 iiiiiiii 101110EE
		else if( (dbmf&0xff0000) == 0x000000 && (op&0xfc) == 0xb8 )
		{
			const TWord ee		= dbmfop&0x000003;
			const TWord iiiiii	= ((dbmfop&0x00ff00)>>8);

			TReg8 val = decode_EE_read(ee);
			val.var &= iiiiii;
			decode_EE_write(ee,val);
		}

		// Lock Instruction Cache Sector

		// PLOCK ea - 0 0 0 0 1 0 1 1 1 1 M M M R R R 1 0 0 0 0 0 0 1
		else if( (dbmf&0xffc000) == 0x0bc000 && op == 0x81 )
		{
			cache.plock( fetchPC() );
		}

		// Unlock Instruction Cache Sector

		// PUNLOCK ea - 0 0 0 0 1 0 1 0 1 1 M M M R R R 1 0 0 0 0 0 0 1
		else if( (dbmf&0xffc000) == 0x0ac000 && op == 0x81 )
		{
			LOG_ERR_NOTIMPLEMENTED("PUNLOCK");
		}

		// Subtract

		// SUB #xx,D - 00000001 01iiiiii 1000d100
		else if( (dbmf&0xffc000) == 0x014000 && (op&0xf7) == 0x84 )
		{
			const bool	ab		= bittest( dbmfop, 3 );
			const TWord	iiiiii	= (dbmfop&0x003f00)>>8;

			alu_sub( ab, TReg56(iiiiii) );
		}

		// SUB #xxxx,D - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 0 1 1 0 0 d 1 0 0
		else if( (dbmf&0xffff00) == 0x014000 && (op&0xf7) == 0xc4 )
		{
			const bool	ab		= bittest( dbmfop, 3 );

			TReg56 r56;
			convert( r56, TReg24(fetchPC()) );

			alu_sub( ab, r56 );
		}

		// Transfer Conditionally

		// Tcc S1,D1 - 00000010 CCCC0000 0JJJd000
		else if( (dbmf&0xff0f00) == 0x020000 && (op&0x87) == 0x00 )
		{
			const TWord cccc = (dbmfop&0x00f000)>>12;

			if( decode_cccc( cccc ) )
			{
				const TWord JJJ = (dbmfop&0x0070)>>4;
				const bool ab = bittest(dbmfop,3);

				decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
			}
		}

		// Tcc S1,D1 S2,D2 - 00000011 CCCC0ttt 0JJJdTTT
		else if( (dbmf&0xff0800) == 0x030000 && (op&0x80) == 0x00 )
		{
			const TWord CCCC	= (dbmfop&0x00f000)>>12;

			if( decode_cccc( CCCC ) )
			{
				const TWord TTT		= dbmfop&0x000007;
				const TWord JJJ		= (dbmfop>>4)&0x7;
				const TWord ttt		= (dbmfop>>8)&7;
				const bool	ab		= bittest( dbmfop, 3 );

				decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
				reg.r[TTT] = reg.r[ttt];
			}
		}

		// Tcc S2,D2 - 00000010 CCCC1ttt 00000TTT
		else if( (dbmf&0xff0800) == 0x020800 && (op&0xf8) == 0x00 )
		{
			const TWord ttt		= (dbmfop&0x000700)>>8;
			const TWord TTT		= (dbmfop&0x000007);
			const TWord CCCC	= (dbmfop&0x00f000)>>12;

			if( decode_cccc( CCCC ) )
				reg.r[TTT] = reg.r[ttt];
		}

		// Viterbi Shift Left

		// VSL S,i,L:ea - 0 0 0 0 1 0 1 S 1 1 M M M R R R 1 1 0 i 0 0 0 0
		else if( (dbmf&0xfec000) == 0x0ac000 && (op&0xef) == 0xc0 )
		{
			LOG_ERR_NOTIMPLEMENTED("VSL");
		}
		else
			return false;
		return true;
	}

	// _____________________________________________________________________________
	// exec_pcu
	//
	bool DSP::exec_pcu( TWord dbmfop, TWord dbmf, TWord op )
	{
		// Bcc, #xxxx			- 00000101 CCCC01aa aa0aaaaa - branch conditionally
		// Bcc, #xxx
		if( (dbmf&0xff0c00) == 0x050400 && (op&0x20) == 0x00 )
		{
			const TWord cccc	= (dbmfop & 0x00f000) >> 12;

			if( decode_cccc( cccc ) )
			{
				const TWord a		= (dbmfop & 0x00001f) | ((dbmfop & 0x0003c0)>>1);

				const TWord disp	= signextend<int,9>( a );
				assert( disp >= 0 );

				reg.pc.var += (disp-1);
			}
		}

		// Bcc, Rn				- 00001101 00011RRR 0100CCCC - branch conditionally
		else if( (dbmf&0xfff800) == 0x0d1800 && (op&0xf0) == 0x40 )
		{
			LOG_ERR_NOTIMPLEMENTED("Bcc");
		}

		// Branch always

		// BRA xxxx - 0 0 0 0 1 1 0 1 0 0 0 1 0 0 0 0 1 1 0 0 0 0 0 0
		else if( (dbmf&0xffff00) == 0x0d1000 && op == 0xc0 )
		{
			const int displacement = signextend<int,24>( fetchPC() );
			reg.pc.var += displacement - 2;
		}

		// BRA xxx - 00000101 000011aa aa0aaaaa
		else if( (dbmf&0xfffc00) == 0x050c00 && (op&0x20) == 0x00 )
		{
			const TWord addr = (dbmfop & 0x1f) | ((dbmfop & 0x3c0)>>1);

			const int signedAddr = signextend<int,9>(addr);

			reg.pc.var += (signedAddr-1);	// PC has already been incremented so subtract 1
		}

		// BRA Rn - 0 0 0 0 1 1 0 1 0 0 0 1 1 R R R 1 1 0 0 0 0 0 0
		else if( (dbmf&0xfff800) == 0x0d1800 && op == 0xc0 )
		{
			LOG_ERR_NOTIMPLEMENTED("BRA");
		}

		// Branch to Subroutine Conditionally

		// BScc xxxx - 0 0 0 0 1 1 0 1 0 0 0 1 0 0 0 0 0 0 0 0 C C C C
		else if( (dbmf&0xffff00) == 0x0d1000 && (op&0xf0) == 0x00 )
		{
			const TWord cccc = dbmfop&0x00000f;

			const int displacement = signextend<int,24>(fetchPC());

			if( decode_cccc(cccc) )
			{
				jsr( reg.pc.var + displacement - 2 );
				LOGSC("BScc xxxx");
			}
		}

		// BScc xxx - 00000101 CCCC00aa aa0aaaaa
		else if( (dbmf&0xff0c00) == 0x050000 && (op&0x20) == 0x00 )
		{
			const TWord cccc = (dbmfop&0x00f000)>>12;

			if( decode_cccc(cccc) )
			{
				const TWord addr = (dbmfop & 0x1f) | ((dbmfop & 0x1c0)>>1);

				int signedAddr = signextend<int,9>(addr);

				// PC has already been incremented so subtract 1
				jsr( reg.pc.var + signedAddr-1 );
				LOGSC("BScc xxx");
			}
		}

		// BScc Rn - 0 0 0 0 1 1 0 1 0 0 0 1 1 R R R 0 0 0 0 C C C C
		else if( (dbmf&0xfff800) == 0x0d1800 && (op&0xf0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("BScc");
		}

		// Branch to Subroutine

		// BSR xxxx - 0 0 0 0 1 1 0 1 0 0 0 1 0 0 0 0 1 0 0 0 0 0 0 0
		else if( (dbmf&0xffff00) == 0xd1000 && op == 0x80 )
		{
			const int displacement = signextend<int,24>(fetchPC());
			jsr( reg.pc.var + displacement - 2 );
		}

		// BSR xxx - 0 0 0 0 0 1 0 1 0 0 0 0 1 0 a a a a 0 a a a a a
		else if( (dbmf&0xfffc00) == 0x50800 && (op&0x20) == 0x00 )
		{
			const TWord aaaaaaaaa = (dbmfop&0x00001f) | ((dbmfop&0x0003c0)>>1);

			const int shortDisplacement = signextend<int,9>(aaaaaaaaa);

			jsr( TReg24(reg.pc.var + shortDisplacement - 1) );
			LOGSC("bsr xxx");
		}

		// BSR Rn - 0 0 0 0 1 1 0 1 0 0 0 1 1 R R R 1 0 0 0 0 0 0 0
		else if( (dbmf&0xfff800) == 0xd1800 && op == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("BSR");
		}

		// Enter Debug Mode

		// DEBUG - 
		else if( (dbmf&0xffff00) == 0x000200 && op == 0x00 )
		{
			LOG( "Entering DEBUG mode" );
			LOG_ERR_NOTIMPLEMENTED("DEBUG");
		}

		// Enter Debug Mode Conditionally

		// DEBUGcc - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 0 0 C C C C
		else if( (dbmf&0xffff00) == 0x000300 && (op&0xf0) == 0x00 )
		{
			const TWord cccc = dbmfop&0x0000f;
			if( decode_cccc( cccc ) )
			{
				LOG( "Entering DEBUG mode because condition is met" );
			}
			LOG_ERR_NOTIMPLEMENTED("DEBUG");
		}

		// Jump Conditionally

		// Jcc xxx - 00001110 CCCCaaaa aaaaaaaa
		else if( (dbmf&0xff0000) == 0x0e0000 )
		{
			const TWord cccc	= (dbmfop&0x00f000)>>12;

			if( decode_cccc( cccc ) )
			{
				const TWord ea = (dbmfop&0x000fff);
				reg.pc.var = ea;
			}
		}

		// Jcc ea - 00001010 11MMMRRR 1010CCCC
		else if( (dbmf&0xffc000) == 0x0ac000 && (op&0xf0) == 0xa0 )
		{
			const TWord cccc	= (dbmfop&0x0000f);
			const TWord mmmrrr	= (dbmfop&0x003f00)>>8;

			const TWord ea		= decode_MMMRRR_read( mmmrrr );

			if( decode_cccc( cccc ) )
			{
				reg.pc.var = ea;
			}
		}

		// Jump

		// JMP ea - 0 0 0 0 1 0 1 0 1 1 M M M R R R 1 0 0 0 0 0 0 0
		else if( (dbmf&0xffc000) == 0x0ac000 && op == 0x80 )
		{
			const TWord mmmrrr = (dbmfop & 0x003f00) >> 8;

			reg.pc.var = decode_MMMRRR_read(mmmrrr);
		}

		// JMP xxx - 00001100 0000aaaa aaaaaaaa
		else if( (dbmf&0xfff000) == 0x0c0000 )
		{
			TWord addr = dbmfop & 0x000fff;
			reg.pc.var = addr;
		}

		// Jump to Subroutine Conditionally

		// JScc xxx - 00001111 CCCCaaaa aaaaaaaa
		else if( (dbmf&0xff0000) == 0x0f0000 )
		{
			const TWord cccc	= (dbmfop&0x00f000)>>12;

			if( decode_cccc( cccc ) )
			{
				const TWord a = dbmfop&0x000fff;
				jsr(a);
			}
		}

		// JScc ea - 00001011 11MMMRRR 1010CCCC
		else if( (dbmf&0xffc000) == 0x0bc000 && (op&0xf0) == 0xa0 )
		{
			const TWord cccc	= (dbmfop&0x0000f);
			const TWord mmmrrr	= (dbmfop&0x003f00)>>8;
			const TWord ea		= decode_MMMRRR_read( mmmrrr );

			if( decode_cccc( cccc ) )
			{
				jsr(TReg24(ea));
				LOGSC("JScc ea");
			}
		}

		// Jump to Subroutine if Bit Clear

		// JSCLR #n,[X or Y]:ea,xxxx - 0 0 0 0 1 0 1 1 0 1 M M M R R R 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0b4000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
		}

		// JSCLR #n,[X or Y]:aa,xxxx - 0 0 0 0 1 0 1 1 0 0 a a a a a a 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0b0000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
		}

		// JSCLR #n,[X or Y]:pp,xxxx - 0 0 0 0 1 0 1 1 1 0 p p p p p p 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0b8000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
		}

		// JSCLR #n,[X or Y]:qq,xxxx - 0 0 0 0 0 0 0 1 1 1 q q q q q q 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x01c000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
		}

		// JSCLR #n,S,xxxx - 0 0 0 0 1 0 1 1 1 1 D D D D D D 0 0 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0bc000 && (op&0xe0) == 0x00 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
		}

		// Jump if Bit Clear

		// WARNING! Documentation seems to be wrong here, we need 5 bits for b, doc says 0bbbb

		// JCLR #n,[X or Y]:ea,xxxx - 00001010 01MMMRRR 1S0bbbbb
		else if( (dbmf&0xffc000) == 0x0a4000 && (op&0xa0) == 0x80 )
		{
			const TWord bit		= (dbmfop&0x00001f);
			const TWord mmmrrr	= (dbmfop&0x003f00)>>8;
			const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_X;
			const TWord addr	= fetchPC();

			const TWord ea		= decode_MMMRRR_read(mmmrrr);

			if( !bittest( ea, bit ) )
			{
				reg.pc.var = addr;
			}

			LOG_ERR_NOTIMPLEMENTED("JCLR");
		}

		// JCLR #n,[X or Y]:aa,xxxx - 0 0 0 0 1 0 1 0 0 0 a a a a a a 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0a0000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("JCLR");
		}

		// JCLR #n,[X or Y]:pp,xxxx - 0 0 0 0 1 0 1 0 1 0 p p p p p p 1 S 0 b b b b b
		else if( (dbmf&0xffc000) == 0x0a8000 && (op&0xa0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("JCLR");
		}

		// JCLR #n,[X or Y]:qq,xxxx - 00000001 10qqqqqq 1S0bbbbb
		else if( (dbmf&0xffc000) == 0x018000 && (op&0xa0) == 0x80 )
		{
			const TWord qqqqqq	= (dbmfop&0x003f00)>>8;
			const TWord bit		= dbmfop & 0x1f;
			const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_X;

			const TWord ea		= 0xffff80 + qqqqqq;

			const TWord addr	= fetchPC();

			if( !bittest( memRead(S, ea), bit ) )
				reg.pc.var = addr;
		}

		// JCLR #n,S,xxxx - 00001010 11DDDDDD 000bbbbb
		else if( (dbmf&0xffc000) == 0x0ac000 && (op&0xe0) == 0x00 )
		{
			const TWord dddddd	= (dbmfop & 0x003f00) >> 8;
			const TWord bit		= dbmfop & 0x1f;

			const TWord addr = fetchPC();

			if( !bittest( decode_dddddd_read(dddddd), bit ) )
				reg.pc.var = addr;
		}

		// Jump if Bit Set

		// JSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 0 1 0 0 1 M M M R R R 1 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0a4000 && (op&0xa0) == 0xa0 )
		{
			const TWord bit		= dbmfop & 0x1f;
			const TWord mmmrrr	= (dbmfop&0x003f00)>>8;
			const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_X;

			const TWord val		= memRead( S, decode_MMMRRR_read( mmmrrr ) );

			if( bittest(val,bit) )
			{
				reg.pc.var = val;
			}
		}

		// JSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 0 1 0 0 0 a a a a a a 1 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0a0000 && (op&0xa0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSET #n,[X or Y]:aa,xxxx");
		}

		// JSET #n,[X or Y]:pp,xxxx - 0 0 0 0 1 0 1 0 1 0 p p p p p p 1 S 1 b b b b b
		else if( (dbmf&0xffc000) == 0x0a8000 && (op&0xa0) == 0xa0 )
		{
			const TWord qqqqqq	= (dbmfop&0x003f00)>>8;
			const TWord bit		= dbmfop & 0x1f;
			const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_X;

			const TWord ea		= 0xFFFFC0 + qqqqqq;

			const TWord addr	= fetchPC();

			if( bittest( memRead(S, ea), bit ) )
				reg.pc.var = addr;
		}

		// JSET #n,[X or Y]:qq,xxxx - 00000001 10qqqqqq 1S1bbbbb
		else if( (dbmf&0xffc000) == 0x018000 && (op&0xa0) == 0xa0 )
		{
			const TWord qqqqqq	= (dbmfop&0x003f00)>>8;
			const TWord bit		= dbmfop & 0x1f;
			const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_X;

			const TWord ea		= 0xFFFF80 + qqqqqq;

			const TWord addr	= fetchPC();

			if( bittest( memRead(S, ea), bit ) )
				reg.pc.var = addr;
		}

		// JSET #n,S,xxxx - 00001010 11DDDDDD 001bbbbb
		else if( (dbmf&0xffc000) == 0x0ac000 && (op&0xe0) == 0x20 )
		{
			const TWord bit		= dbmfop & 0x1f;
			const TWord dddddd	= (dbmfop&0x003f00)>>8;

			const TWord addr	= fetchPC();

			const TReg24 var	= decode_dddddd_read( dddddd );

			if( bittest(var,bit) )
			{
				reg.pc.var = addr;
			}
		}

		// Jump to Subroutine

		// JSR ea - 0 0 0 0 1 0 1 1 1 1 M M M R R R 1 0 0 0 0 0 0 0
		else if( (dbmf&0xffc000) == 0x0bc000 && op == 0x80 )
		{
			const TWord mmmrrr = ((dbmfop&0x003f00)>>8);

			const TWord ea = decode_MMMRRR_read( mmmrrr );

			jsr(TReg24(ea));
			LOGSC("jsr ea");
		}

		// JSR xxx - 0 0 0 0 1 1 0 1 0 0 0 0 a a a a a a a a a a a a
		else if( (dbmf&0xfff000) == 0x0d0000 )
		{
			const TWord ea = (dbmfop&0x000fff);
			jsr(TReg24(ea));
			LOGSC("jsr xxx");
		}

		// Jump to Subroutine if Bit Set

		// JSSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 0 1 1 0 1 M M M R R R 1 S 1 0 b b b b
		else if( (dbmf&0xffc000) == 0x0b4000 && (op&0xb0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSSET");
		}

		// JSSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 0 1 1 0 0 a a a a a a 1 S 1 0 b b b b
		else if( (dbmf&0xffc000) == 0x0b0000 && (op&0xb0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSSET");
		}

		// JSSET #n,[X or Y]:pp,xxxx - 0 0 0 0 1 0 1 1 1 0 p p p p p p 1 S 1 0 b b b b
		else if( (dbmf&0xffc000) == 0x0b8000 && (op&0xb0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSSET");
		}

		// JSSET #n,[X or Y]:qq,xxxx - 0 0 0 0 0 0 0 1 1 1 q q q q q q 1 S 1 0 b b b b
		else if( (dbmf&0xffc000) == 0x01c000 && (op&0xb0) == 0xa0 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSSET");
		}

		// JSSET #n,S,xxxx - 0 0 0 0 1 0 1 1 1 1 D D D D D D 0 0 1 0 b b b b
		else if( (dbmf&0xffc000) == 0x0bc000 && (op&0xf0) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("JSSET");
		}

		// Repeat Next Instruction

		// REP [X or Y]:ea - 0 0 0 0 0 1 1 0 0 1 M M M R R R 0 S 1 0 0 0 0 0
		else if( (dbmf&0xffc000) == 0x064000 && (op&0xbf) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("REP");
		}

		// REP [X or Y]:aa - 0 0 0 0 0 1 1 0 0 0 a a a a a a 0 S 1 0 0 0 0 0
		else if( (dbmf&0xffc000) == 0x060000 && (op&0xbf) == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("REP");
		}

		// REP #xxx - 00000110 iiiiiiii 1010hhhh
		else if( (dbmf&0xff0000) == 0x060000 && (op&0xf0) == 0xa0 )
		{
			const TWord loopcount = ((dbmfop&0x00000f) << 8) | ((dbmfop&0x00ff00)>>8);

			repRunning = true;
			tempLCforRep = reg.lc;

			reg.lc.var = loopcount;
		}

		// REP S - 0000011011dddddd00100000
		else if( (dbmf&0xffc000) == 0x06c000 && op == 0x20 )
		{
			LOG_ERR_NOTIMPLEMENTED("REP");
		}

		// Reset On-Chip Peripheral Devices

		// RESET - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 1 0 0
		else if( dbmfop == 0x84 )
		{
			resetSW();
		}

		else
			return false;
		return true;
	}

	// _____________________________________________________________________________
	// decode_MMMRRR
	//
	dsp56k::TWord DSP::decode_MMMRRR_read( TWord _mmmrrr )
	{
		switch( _mmmrrr & 0x3f )
		{
		case 0x30:	return fetchPC();		// absolute address
		case 0x34:	return fetchPC();		// immediate data
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
bool DSP::exec_move( TWord dbmfop, TWord dbmf, TWord op )
{
	// X Memory Data Move

	// MOVE X:(Rn + xxxx),D - 00001010 01110RRR 1WDDDDDD
	// MOVE S,X:(Rn + xxxx)
	if( (dbmf&0xfff800) == 0x0a7000 && (op&0x80) == 0x80 )
	{
		const TWord DDDDDD	= (dbmfop&0x00003f);
		const bool	write	= (dbmfop&0x000040) != 0;
		const TWord rrr		= (dbmfop&0x000700)>>8;

		const int shortDisplacement = signextend<int,24>(fetchPC());
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		if( write )
		{
			decode_dddddd_write( DDDDDD, TReg24(memRead( MemArea_X, ea )) );
		}
		else
		{
			memWrite( MemArea_X, ea, decode_dddddd_read( DDDDDD ).var );
		}
	}

	// MOVE X:(Rn + xxx),D - 0000001a aaaaaRRR 1a0WDDDD
	// MOVE S,X:(Rn + xxx)
	else if( (dbmf&0xfe0000) == 0x020000 && (op&0xa0) == 0x80 )
	{
		const TWord ddddd	= (dbmfop&0x00000f);
		const TWord aaaaaaa	= ((dbmfop&0x01f800)>>10) | ((dbmfop&0x000040)>>6);
		const bool	write	= (dbmfop&0x000010) != 0;
		const TWord rrr		= (dbmfop&0x000700)>>8;

		const int shortDisplacement = signextend<int,7>(aaaaaaa);
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		if( write )
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( MemArea_X, ea )) );
		}
		else
		{
			memWrite( MemArea_X, ea, decode_ddddd_read<TWord>( ddddd ) );
		}
	}

	// Y Memory Data Move

	// MOVE Y:(Rn + xxxx),D - 00001011 01110RRR 1WDDDDDD
	// MOVE D,Y:(Rn + xxxx)
	else if( (dbmf&0xfff800) == 0x0b7000 && (op&0x80) == 0x80 )
	{
		const TWord ddddd	= (dbmfop&0x00003f);
		const bool	write	= (dbmfop&0x000040) != 0;
		const TWord rrr		= (dbmfop&0x000700)>>8;

		const int shortDisplacement = signextend<int,24>(fetchPC());
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		if( write )
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( MemArea_Y, ea )) );
		}
		else
		{
			memWrite( MemArea_Y, ea, decode_ddddd_read<TWord>( ddddd ) );
		}
	}

	// MOVE Y:(Rn + xxx),D - 0000001a aaaaaRRR 1a1WDDDD
	// MOVE D,Y:(Rn + xxx)
	else if( (dbmf&0xfe0000) == 0x020000 && (op&0xa0) == 0xa0 )
	{
		const TWord ddddd	= (dbmfop&0x00000f);
		const TWord aaaaaaa	= ((dbmfop&0x01f800)>>10) | ((dbmfop&0x000040)>>6);
		const bool	write	= (dbmfop&0x000010) != 0;
		const TWord rrr		= (dbmfop&0x000700)>>8;

		const int shortDisplacement = signextend<int,7>(aaaaaaa);
		const TWord ea = decode_RRR_read( rrr, shortDisplacement );

		if( write )
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(memRead( MemArea_Y, ea )) );
		}
		else
		{
			memWrite( MemArea_Y, ea, decode_ddddd_read<TWord>( ddddd ) );
		}
	}

	// Move Control Register

	// MOVE(C) [X or Y]:ea,D1 - 0 0 0 0 0 1 0 1 W 1 M M M R R R O S 1 d d d d d
	// MOVE(C) S1,[X or Y]:ea
	// MOVE(C) #xxxx,D1
	else if( (dbmf&0xff4000) == 0x054000 && (op&0xa0) == 0x20 )//if( (dbmf&0xff4000) == 0x054000 && (op&0x20) == 0x20 )
	{
		const TWord mmmrrr = (dbmf>>8) & 0x3f;

		const TWord addr = decode_MMMRRR_read( mmmrrr );

		const bool write = (dbmf & 0x8000) != 0;

		const EMemArea area = ((op&40) != 0) ? MemArea_Y : MemArea_X;

		// TODO: what is 'O' good for? isn't mentioned in the docs at MOVEC?! I assume it's a typo and it should be a zero bit
		const bool o = (op&0x80) != 0;
		assert( o == false );

		if( write )
		{
			if( mmmrrr == 0x34 )	decode_ddddd_pcr_write( op&0x1f, TReg24(addr) );		
			else					decode_ddddd_pcr_write( op&0x1f, TReg24(memRead( area, addr )) );
		}
		else
		{
			const TReg24 regVal = decode_ddddd_pcr_read(op&0x1f);
			assert( (mmmrrr != 0x34) && "register move to immediate data? not possible" );
			memWrite( area, addr, regVal.toWord() );
		}
	}

	// MOVE(C) [X or Y]:aa,D1 - 0 0 0 0 0 1 0 1 W 0 a a a a a a 0 S 1 d d d d d
	// MOVE(C) S1,[X or Y]:aa
	else if( (dbmf&0xff4000) == 0x050000 && (op&0xa0) == 0x20 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE");
	}

	// MOVE(C) S1,D2 - 0 0 0 0 0 1 0 0 W 1 e e e e e e 1 0 1 d d d d d
	// MOVE(C) S2,D1
	else if( (dbmf&0xff4000) == 0x044000 && (op&0xe0) == 0xa0 )
	{
		const bool write = (dbmf & 0x8000) != 0;

		const TWord eeeeee	= (dbmfop & 0x003f00) >> 8;
		const TWord ddddd	= (dbmfop & 0x00001f);

		if( write )
			decode_ddddd_pcr_write( ddddd, decode_dddddd_read( eeeeee ) );
		else
			decode_dddddd_write( eeeeee, decode_ddddd_pcr_read( ddddd ) );
	}

	// MOVE(C) #xx,D1 - 0 0 0 0 0 1 0 1 i i i i i i i i 1 0 1 d d d d d
	else if( (dbmf&0xff0000) == 0x050000 && (op&0xe0) == 0xa0 )
	{
		const TWord data  = (dbmfop&0x00ff00) >> 8;
		const TWord ddddd = dbmfop&0x00001f;

		decode_ddddd_pcr_write( ddddd, TReg24(data) );
	}

	// Move Program Memory

	// MOVE(M) S,P:ea - 0 0 0 0 0 1 1 1 W 1 M M M R R R 1 0 d d d d d d
	// MOVE(M) P:ea,D
	else if( (dbmf&0xff4000) == 0x074000 && (op&0xc0) == 0x80 )
	{
		const bool	write	= (dbmfop&0x008000) != 0;
		const TWord dddddd	= (dbmfop&0x00003f);
		const TWord mmmrrr	= (dbmfop&0x003f00)>>8;

		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		if( write )
		{
			assert( mmmrrr != 0x34 && "immediate data should not be allowed here" );
			decode_dddddd_write( dddddd, TReg24(memRead( MemArea_P, ea )) );
		}
		else
		{
			memWrite( MemArea_P, ea, decode_dddddd_read(dddddd).toWord() );
		}
	}

	// MOVE(M) S,P:aa - 0 0 0 0 0 1 1 1 W 0 a a a a a a 0 0 d d d d d d
	// MOVE(M) P:aa,D
	else if( (dbmf&0xff4000) == 0x070000 && (op&0xc0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE(M) S,P:aa");
	}

	// Move Peripheral Data

	// MOVEP [X or Y]:pp,[X or Y]:ea - 0 0 0 0 1 0 0 s W 1 M M M R R R 1 S p p p p p p
	// MOVEP [X or Y]:ea,[X or Y]:pp
	else if( (dbmf&0xfe4000) == 0x084000 && (op&0x80) == 0x80 )
	{
		const TWord pp		= 0xFFFFC0 + (dbmfop & 0x00003f);
		
		const TWord mmmrrr	= (dbmfop & 0x003f00) >> 8;
		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		const bool write	= (dbmfop & 0x008000) != 0;
		const EMemArea s	= (dbmfop & 0x010000) ? MemArea_Y : MemArea_X;
		const EMemArea S	= (dbmfop & 0x000040) ? MemArea_Y : MemArea_X;

		if( write )
		{
			if( mmmrrr == 0x34 )
				memWrite( S, pp, ea );
			else
				memWrite( S, pp, memRead( s, ea ) );
		}
		else
			memWrite( s, ea, memRead( S, pp ) );
	}

	// MOVEP X:qq,[X or Y]:ea - 0 0 0 0 0 1 1 1 W 1 M M M R R R 0 S q q q q q q
	// MOVEP [X or Y]:ea,X:qq
	else if( (dbmf&0xff4000) == 0x074000 && (op&0x80) == 0x00 )
	{
		const TWord mmmrrr	= (dbmfop & 0x003f00) >> 8;
		const EMemArea S	= (dbmfop & 0x000040) ? MemArea_Y : MemArea_X;
		const TWord qAddr	= (dbmfop & 0x00003f) + 0xffff80;
		const bool write	= (dbmfop & 0x008000) != 0;

		const TWord ea		= decode_MMMRRR_read( mmmrrr );

		if( write )
		{
			if( mmmrrr == 0x34 )
				memWrite( MemArea_X, qAddr, ea );
			else
				memWrite( MemArea_X, qAddr, memRead( S, ea ) );
		}
		else
			memWrite( S, ea, memRead( MemArea_X, qAddr ) );
	}

	// MOVEP Y:qq,[X or Y]:ea - 0 0 0 0 0 1 1 1 W 0 M M M R R R 1 S q q q q q q
	// MOVEP [X or Y]:ea,Y:qq
	else if( (dbmf&0xff4000) == 0x070000 && (op&0x80) == 0x80 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE");
	}

	// MOVEP P:ea,[X or Y]:pp - 0 0 0 0 1 0 0 s W 1 M M M R R R 0 1 p p p p p p
	// MOVEP [X or Y]:pp,P:ea
	else if( (dbmf&0xfe4000) == 0x084000 && (op&0xc0) == 0x40 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE");
	}

	// MOVEP P:ea,[X or Y]:qq - 0 0 0 0 0 0 0 0 1 W M M M R R R 0 S q q q q q q
	// MOVEP [X or Y]:qq,P:ea
	else if( (dbmf&0xff8000) == 0x008000 && (op&0x80) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE");
	}

	// MOVEP S,[X or Y]:pp - 0 0 0 0 1 0 0 s W 1 d d d d d d 0 0 p p p p p p
	// MOVEP [X or Y]:pp,D
	else if( (dbmf&0xfe4000) == 0x084000 && (op&0xc0) == 0x00 )
	{
		const TWord addr	= 0xffffc0 + (dbmfop & 0x3f);
		const TWord dddddd	= (dbmfop & 0x003f00) >> 8;

		const EMemArea area = (dbmfop & 0x010000) ? MemArea_Y : MemArea_X;
		const bool	write	= (dbmfop & 0x008000) != 0;

		if( write )
			decode_dddddd_write( dddddd, TReg24(memRead( area, addr )) );
		else
			memWrite( area, addr, decode_dddddd_read( dddddd ).toWord() );
	}

	// MOVEP S,X:qq - 0 0 0 0 0 1 0 0 W 1 d d d d d d 1 q 0 q q q q q
	// MOVEP X:qq,D
	else if( (dbmf&0xff4000) == 0x044000 && (op&0xa0) == 0x80 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE");
	}

	// MOVEP S,Y:qq - 0 0 0 0 0 1 0 0 W 1 d d d d d d 0 q 1 q q q q q
	// MOVEP Y:qq,D
	else if( (dbmf&0xff4000) == 0x044000 && (op&0xa0) == 0x20 )
	{
		LOG_ERR_NOTIMPLEMENTED("MOVE");
	}
	else
		return false;
	return true;
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
	case 0x0a:	{ TReg24 res; convert(res,a2()); return res; }
	case 0x0b:	{ TReg24 res; convert(res,b2()); return res; }
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
bool DSP::exec_operand_8bits(TWord dbmf, TWord op)
{
	if( (dbmf&0xffff00) != 0x000000 )
		return false;

	// Return From Interrupt

	// RTI - 000000000000000000000100
	if( op == 0x04 )
	{
		popPCSR();
		LOGSC("rti");
	}

	// End Current DO Loop

	// ENDDO - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 1 0 0
	else if( op == 0x8c )
	{
		// restore previous loop flag
		sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

		// decrement SP twice, restoring old loop settings
		decSP();

		reg.lc = ssl();
		reg.la = ssh();

		return true;
	}

	// Decrement by One

	// DEC D - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 d
	else if( (op&0xfe) == 0x0a )
	{
		TReg56& d = bittest(op,0) ? reg.b : reg.a;

		const TReg56 old = d;

		const TInt64 res = --d.var;

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

	// Increment by One

	// INC D - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 d
	else if( (op&0xfe) == 0x08 )
	{
		TReg56& d = bittest(op,0) ? reg.b : reg.a;

		const TReg56 old = d;

		const TInt64 res = ++d.var;

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

	// Unlock Instruction Cache Relative Sector

	// PUNLOCKR xxxx - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 0
	else if( op == 0x0e )
	{
		LOG_ERR_NOTIMPLEMENTED("PUNLOCKR");
	}

	// Program Cache Flush

	// PFLUSH - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1
	else if( op == 0x03 )
	{
		LOG_ERR_NOTIMPLEMENTED("PFLUSH");
	}

	// Program Cache Flush Unlocked Sections

	// PFLUSHUN - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1
	else if( op == 0x01 )
	{
		cache.pflushun();
	}

	// Program Cache Global Unlock

	// PFREE - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0
	else if( op == 0x02 )
	{
		cache.pfree();
	}

	// Lock Instruction Cache Relative Sector

	// PLOCKR xxxx - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1
	else if( op == 0x0f )
	{
		LOG_ERR_NOTIMPLEMENTED("PLOCKR");
	}

	// Illegal Instruction Interrupt

	// ILLEGAL - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1
	else if( op == 0x05 )
	{
		LOG_ERR_NOTIMPLEMENTED("ILLEGAL");
	}

	// Return From Subroutine

	// RTS - 000000000000000000001100
	else if( op == 0x0c )
	{
		popPC();
		LOGSC( "RTS" );
	}

	// Stop Instruction Processing

	// STOP - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 1 1 1
	else if( op == 0x87 )
	{
		LOG_ERR_NOTIMPLEMENTED("STOP");
	}

	// Software Interrupt

	// TRAP - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0
	else if( op == 0x06 )
	{
		LOG_ERR_NOTIMPLEMENTED("TRAP");
	}

	// Wait for Interrupt or DMA Request

	// WAIT - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 1 1 0
	else if( op == 0x86 )
	{
		LOG_ERR_NOTIMPLEMENTED("WAIT");
	}

	// Conditional Software Interrupt

	// TRAPcc - 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 C C C C
	else if( (op&0xf0) == 0x10 )
	{
		LOG_ERR_NOTIMPLEMENTED("TRAPcc");
	}
	else
		return false;
	return true;
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
	if( write && mmmrrr == 0x34 )
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
bool DSP::exec_bitmanip( TWord dbmfop, TWord dbmf, TWord op )
{
	// Bit Set and Test

	// BSET #n,[X or Y]:ea - 0 0 0 0 1 0 1 0 0 1 M M M R R R 0 S 1 b b b b b
	if( (dbmf&0xffc000) == 0x0a4000 && (op&0xa0) == 0x20 )
	{
		const TWord bit		= dbmfop&0x00001f;

		const TWord mmmrrr	= (dbmfop&0x003f00)>>8;

		const TWord ea		= decode_MMMRRR_read(mmmrrr);

		const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_Y;

		TWord val = memRead( S, ea );

		sr_toggle( SR_C, bittestandset( val, bit ) );

		memWrite( S, ea, val );
	}

	// BSET #n,[X or Y]:aa - 0 0 0 0 1 0 1 0 0 0 a a a a a a 0 S 1 b b b b b
	else if( (dbmf&0xffc000) == 0x0a0000 && (op&0xa0) == 0x20 )
	{
		LOG_ERR_NOTIMPLEMENTED("BSET");
	}

	// BSET #n,[X or Y]:pp - 0 0 0 0 1 0 1 0 1 0 p p p p p p 0 S 1 b b b b b
	else if( (dbmf&0xffc000) == 0x0a8000 && (op&0xa0) == 0x20 )
	{
		LOG_ERR_NOTIMPLEMENTED("BSET");
	}

	// BSET #n,[X or Y]:qq - 0 0 0 0 0 0 0 1 0 0 q q q q q q 0 S 1 b b b b b
	else if( (dbmf&0xffc000) == 0x010000 && (op&0xa0) == 0x20 )
	{
		const TWord bit		= dbmfop&0x00001f;

		const TWord qqqqqq	= (dbmfop&0x003f00)>>8;

		const TWord ea		= 0xffff80 + qqqqqq;

		const EMemArea S	= bittest(dbmfop,6) ? MemArea_Y : MemArea_Y;

		TWord val = memRead( S, ea );

		sr_toggle( SR_C, bittestandset( val, bit ) );

		memWrite( S, ea, val );
	}

	// BSET #n,D - 0 0 0 0 1 0 1 0 1 1 D D D D D D 0 1 1 b b b b b
	else if( (dbmf&0xffc000) == 0x0ac000 && (op&0xe0) == 0x60 )
	{
		const TWord bit = (dbmfop&0x1f);
		const TWord d = ((dbmfop&0x003f00) >> 8);

		TReg24 val = decode_dddddd_read(d);

		if( (d & 0x3f) == 0x39 )	// is SR the destination?	
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

	// Bit test and change

	// BCHG #n,[X or Y]:ea	- 00001101 00011RRR 0100CCCC
	else if( (dbmf&0xffc000) == 0x0b4000 && (op&0x30) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("BCHG");
	}

	// BCHG #n,[X or Y]:aa	- bit test and change
	// BCHG #n,[X or Y]:pp
	else if( (dbmf&0xffc000) == 0x0b0000 && (op&0xb0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("BCHG");
	}

	// BCHG #n,[X or Y]:qq	- bit test and change
	else if( (dbmf&0xffc000) == 0x014000 && (op&0xa0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("BCHG");
	}

	// BCHG #n,D			- 00001011 11DDDDDD 010bbbbb
	// 
	else if( (dbmf&0xffc000) == 0x0bc000 && (op&0xe0) == 0x40 )
	{
		const TWord		dddddd	= ((dbmfop&0x003f00)>>8);
		const TWord		bit		= dbmfop&0x00001f;

		TReg24			val		= decode_dddddd_read( dddddd );

		sr_toggle( SR_C, bittestandchange( val, bit ) );

		decode_dddddd_write( dddddd, val );

		sr_s_update();
		sr_l_update_by_v();
	}

	// Bit test and clear

	// BCLR #n,[X or Y]:ea - 0 0 0 0 1 0 1 0 0 1 M M M R R R 0 S 0 b b b b b
	else if( (dbmf&0xffc000) == 0x0a4000 && (op&0xa0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("BCLR");
	}

	// BCLR #n,[X or Y]:aa - 0 0 0 0 1 0 1 0 0 0 a a a a a a 0 S 0 b b b b b
	else if( (dbmf&0xffc000) == 0x0a0000 && (op&0xa0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("BCLR");
	}

	// BCLR #n,[X or Y]:pp - 0 0 0 0 1 0 1 0 1 0 p p p p p p 0 S 0 b b b b b
	else if( (dbmf&0xffc000) == 0x0a8000 && (op&0xa0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("BCLR");
	}

	// BCLR #n,[X or Y]:qq - 0 0 0 0 0 0 0 1 0 0 q q q q q q 0 S 0 b b b b b
	else if( (dbmf&0xffc000) == 0x010000 && (op&0xa0) == 0x00 )
	{
		const TWord bit = (dbmfop&0x1f);
		const TWord ea	= 0xffff80 + ((dbmfop&0x003f00)>>8);

		const EMemArea S = (dbmfop&0x000040) ? MemArea_Y : MemArea_X;

		const TWord res = alu_bclr( bit, memRead( S, ea ) );

		memWrite( S, ea, res );
	}

	// BCLR #n,D - 0 0 0 0 1 0 1 0 1 1 D D D D D D 0 1 0 b b b b b
	else if( (dbmf&0xffc000) == 0x0ac000 && (op&0xe0) == 0x40 )
	{
		const TWord bit		= (dbmfop&0x1f);
		const TWord dddddd	= (dbmfop&0x003f00)>>8;

		TWord val;
		convert( val, decode_dddddd_read( dddddd ) );

		const TWord newVal = alu_bclr( bit, val );
		decode_dddddd_write( dddddd, TReg24(newVal) );
	}

	// Bit Test

	// BTST #n,[X or Y]:ea - 00001011 01MMMRRR OS1bbbbb
	else if( (dbmf&0xffc000) == 0x0b4000 && (op&0x20) == 0x20 )
	{
		const TWord bit = (dbmfop&0x00001f);
		const TWord mmmrrr = (dbmfop&0x003f00)>>8;
		const TWord ea = decode_MMMRRR_read( mmmrrr );
		const EMemArea S = bittest( dbmfop, 6 ) ? MemArea_Y : MemArea_X;

		const TWord val = memRead( S, ea );

		sr_toggle( SR_C, bittest( val, bit ) );

		sr_s_update();
		sr_l_update_by_v();
	}

	// BTST #n,[X or Y]:aa - 0 0 0 0 1 0 1 1 0 0 a a a a a a 0 S 1 b b b b b
	else if( (dbmf&0xffc000) == 0x0b0000 && (op&0xa0) == 0x20 )
	{
		LOG_ERR_NOTIMPLEMENTED("BTST");
	}

	// BTST #n,[X or Y]:pp - 0 0 0 0 1 0 1 1 1 0 p p p p p p 0 S 1 b b b b b
	else if( (dbmf&0xffc000) == 0x0b8000 && (op&0xa0) == 0x20 )
	{
		const TWord bitNum	= dbmfop&0x00000f;
		const TWord pppppp	= (dbmfop&0x003f00) >> 8;

		const EMemArea S	= (dbmfop&0x000040) ? MemArea_Y : MemArea_X;

		const TWord memVal	= memRead( S, pppppp + 0xffffc0 );

		const bool bitSet	= ( memVal & (1<<bitNum)) != 0;

		sr_toggle( SR_C, bitSet );
	}

	// BTST #n,[X or Y]:qq - 0 0 0 0 0 0 0 1 0 1 q q q q q q 0 S 1 b b b b b
	else if( (dbmf&0xffc000) == 0x014000 && (op&0xa0) == 0x20 )
	{
		LOG_ERR_NOTIMPLEMENTED("BTST");
	}

	// BTST #n,D - 00001011 11DDDDDD 011bbbbb
	else if( (dbmf&0xffc000) == 0x0bc000 && (op&0xe0) == 0x60 )
	{
		const TWord dddddd	= (dbmfop&0x003f00)>>8;
		const TWord bit		= dbmfop&0x00001f;

		TReg24 val = decode_dddddd_read( dddddd );

		sr_toggle( SR_C, bittest( val.var, bit ) );
	}
	else
		return false;
	return true;
}
// _____________________________________________________________________________
// exec_logical_nonparallel
//
bool DSP::exec_logical_nonparallel( TWord dbmfop, TWord dbmf, TWord op )
{
	if( (dbmf&0xffff00) == 0x0c1e00 )
	{
		// LSL #ii,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 1 0 i i i i i D
		if( (op&0xc0) == 0x80 )
		{
			LOG_ERR_NOTIMPLEMENTED("LSL");
		}

		// LSL S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 0 1 s s s D
		else if( (op&0xf0) == 0x10 )
		{
			LOG_ERR_NOTIMPLEMENTED("LSL");
		}

		// LSR #ii,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 1 1 i i i i i D
		else if( (op&0xc0) == 0xc0 )
		{
			LOG_ERR_NOTIMPLEMENTED("LSL");
		}

		// LSR S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 1 1 s s s D
		else if( (op&0xf0) == 0x30 )
		{
			LOG_ERR_NOTIMPLEMENTED("LSL");
		}
		else
			return false;
	}

	// Logical Exclusive OR

	// EOR #xx,D - 0 0 0 0 0 0 0 1 0 1 i i i i i i 1 0 0 0 d 0 1 1
	else if( (dbmf&0xffc000) == 0x014000 && (op&0xf7) == 0x83 )
	{
		LOG_ERR_NOTIMPLEMENTED("EOR");
	}

	// EOR #xxxx,D - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 0 1 1 0 0 d 0 1 1
	else if( (dbmf&0xffff00) == 0x014000 && (op&0xf7) == 0xc3 )
	{
		LOG_ERR_NOTIMPLEMENTED("EOR");
	}

	// Extract Bit Field

	// EXTRACT S1,S2,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 1 0 0 0 0 s S S S D
	else if( (dbmf&0xffff00) == 0x0c1a00 && (op&0xe0) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("EXTRACT");
	}

	// EXTRACT #CO,S2,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 0 0 0 0 0 s 0 0 0 D
	else if( (dbmf&0xffff00) == 0x0c1800 && (op&0xee) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("EXTRACT");
	}

	// Extract Unsigned Bit Field

	// EXTRACTU S1,S2,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 1 0 1 0 0 s S S S D
	else if( (dbmf&0xffff00) == 0x0c1a00 && (op&0xe0) == 0x80 )
	{
		LOG_ERR_NOTIMPLEMENTED("EXTRACTU");
	}

	// EXTRACTU #CO,S2,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 0 0 1 0 0 s 0 0 0 D
	else if( (dbmf&0xffff00) == 0x0c1800 && (op&0xee) == 0x80 )
	{
		LOG_ERR_NOTIMPLEMENTED("EXTRACTU");
	}

	// Execute Conditionally Without CCR Update

	// Insert Bit Field

	// INSERT S1,S2,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 1 1 0 q q q S S S D
	else if( (dbmf&0xffff00) == 0x0c1b00 && (op&0x80) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("INSERT");
	}

	// INSERT #CO,S2,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 0 1 0 q q q 0 0 0 D
	else if( (dbmf&0xffff00) == 0x0c1900 && (op&0x8e) == 0x00 )
	{
		LOG_ERR_NOTIMPLEMENTED("INSERT");
	}

	// Merge Two Half Words

	// MERGE S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 0 1 1 1 0 0 0 S S S D
	else if( (dbmf&0xffff00) == 0x0c1b00 && (op&0xf0) == 0x80 )
	{
		LOG_ERR_NOTIMPLEMENTED("MERGE");
	}

	// Logical Inclusive OR

	// OR #xx,D - 0 0 0 0 0 0 0 1 0 1 i i i i i i 1 0 0 0 d 0 1 0
	else if( (dbmf&0xffc000) == 0x014000 && (op&0xf7) == 0x82 )
	{
		LOG_ERR_NOTIMPLEMENTED("OR");
	}

	// OR #xxxx,D - 0 0 0 0 0 0 0 1 0 1 0 0 0 0 0 0 1 1 0 0 d 0 1 0
	else if( (dbmf&0xffff00) == 0x014000 && (op&0xf7) == 0xc2 )
	{
		LOG_ERR_NOTIMPLEMENTED("OR");
	}

	// OR Immediate With Control Register

	// OR(I) #xx,D - 0 0 0 0 0 0 0 0 i i i i i i i i 1 1 1 1 1 0 E E

	else if( (dbmf&0xff0000) == 0x000000 && (op&0xfc) == 0xf8 )
	{
		const TWord iiiiiiii = (dbmfop & 0x00ff00) >> 8;
		const TWord ee = dbmfop&0x3;

		switch( ee )
		{
		case 0:	mr ( TReg8( mr().var | iiiiiiii) );	break;
		case 1:	ccr( TReg8(ccr().var | iiiiiiii) );	break;
		case 2:	com( TReg8(com().var | iiiiiiii) );	break;
		case 3:	eom( TReg8(eom().var | iiiiiiii) );	break;
		}
	}
	else
		return false;
	return true;
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
			reg.pc.var = _addr+1;
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
	reg.pc.var = reg.la.var+1;

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
void DSP::alu_mpy( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _accumulate )
{
//	assert( sr_test(SR_S0) == 0 && sr_test(SR_S1) == 0 );

	const TInt64 s1 = _s1.signextend<TInt64>();
	const TInt64 s2 = _s2.signextend<TInt64>();

	TInt64 res = s1 * s2;

	// needs one shift ("signed fraction")

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
bool DSP::readRegToInt( EReg _reg, signed __int64& _dst )
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

	case Reg_PC:	reg.pc = _res;		break;

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

#define MEM_VALIDRANGE( ADDR0, ADDR1 )			if( _addr >= ADDR0 && _addr < ADDR1 ) { return true; }
#define MEM_INVALIDRANGE( ADDR0, ADDR1, MSG )	if( _addr >= ADDR0 && _addr < ADDR1 ) { assert( 0 && MSG ); return false; }


// _____________________________________________________________________________
// memValidateAccess
//
bool DSP::memValidateAccess( EMemArea _area, TWord _addr, bool _write ) const
{
	// XXX stuff goes first

	// XXX external memory (768k)
	MEM_VALIDRANGE	( 0x040000, 0x060000 );
	MEM_INVALIDRANGE( 0x060000, 0x0c0000, "memory access above external memory limit of XXX PCI/Element, only XXX FW has this memory area" );

	switch( _area )
	{
	case MemArea_P:
		if( !_write )
			MEM_VALIDRANGE( 0x000, 0x300 );

		MEM_VALIDRANGE( 0x300, 0x2000 );

		// according to XXX sdk 0x200-0x2ff are used by PCI os but my test code accesses this area....
		MEM_INVALIDRANGE( 0x000, 0x200, "Access to P-memory that's used by XXX FW&PCI OS" );

		if( _write )
		{
			// Bootstram ROM
			MEM_INVALIDRANGE( 0xff0000, 0xff00c0, "Tried to write to Bootstrap ROM" );
		}
	case MemArea_X:
		MEM_VALIDRANGE( 0x200, 0x1e00 );

		// input
		MEM_VALIDRANGE( 0x1e00, 0x1e02 );

		// output
		MEM_VALIDRANGE( 0x1f00, 0x1f08 );
		
		// The X memory space at locations $001600 to $001FFF and from $ to $FFEFFF is reserved and should not be accessed.
		MEM_INVALIDRANGE( 0x001600, 0x002000, "Access to X memory area that shouldn't be used" );
		MEM_INVALIDRANGE( 0xff0000, 0xffefff, "Access to X memory area that shouldn't be used" );

		if( _write )
		{
			MEM_INVALIDRANGE( 0, 0x200, "Access to X-memory of XXX PCI-OS used memory" );
		}
		MEM_VALIDRANGE( 0xffff80, 0x1000000 );	// peripherals
		break;
	case MemArea_Y:

		MEM_VALIDRANGE( 0x100, 0x2000 );

		if( _write )
		{
			MEM_INVALIDRANGE( 0, 0x100, "Access to X-memory of XXX PCI-OS used memory" );
		}
		MEM_INVALIDRANGE( 0xff0000, 0xffefff, "Access to Y memory area that shouldn't be used" );
		break;
	}
	LOG( "Unknown memory access in area " << _area << ", address " << std::hex << _addr << ", access type " << _write << ", pc " << std::hex << pcLastExec << ", ictr " << std::hex << reg.ictr.var );

	assert( 0 && "unknown memory access" );
	return false;
}

// _____________________________________________________________________________
// memWrite
//
bool DSP::memWrite( EMemArea _area, TWord _offset, TWord _value )
{
	memTranslateAddress(_area,_offset);

	memValidateAccess( _area,_offset,true );

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

	memTranslateAddress(_area,_offset);

	memValidateAccess(_area,_offset,false);

	return mem.get( _area, _offset );
}

// _____________________________________________________________________________
// memTranslateAddress
//
bool DSP::memTranslateAddress( EMemArea& _area, TWord& _offset ) const
{
	// XXX maps external memory to XYP areas at once
	if( _offset >= 0x040000 && _offset < 0x0c0000 && (_area == MemArea_Y || _area == MemArea_P) )
	{
		_area = MemArea_X;
		return true;
	}

	// map Y-memory to P-memory on DSP 56362
	if( _area == MemArea_P )
	{
		const bool ms = (reg.omr.var & OMR_MS) != 0;
		const bool ce = sr_test(SR_CE);

		if( ms )
		{
			if( ce )
			{
				assert( (_offset < 0x1000 || _offset >= 0x1400) && "illegal access to instruction cache" );
			}

			if( _offset >= 0xc00 && _offset < 0x1400 )
			{
				LOG( "Remap address P " << std::hex << _offset << " to Y " << (_offset-0xc00 + 0xe00) );
				_offset -= 0xc00;
				_offset += 0xe00;
				_area = MemArea_Y;
				return true;
			}
		}
		else if( ce )
		{
			assert( (_offset < 0x800 || _offset >= 0xc00) && "illegal access to instruction cache" );
		}
	}

	// address not modified
	return false;
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
