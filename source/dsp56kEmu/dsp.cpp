// DSP 56300 family 24-bit DSP emulator

#include "dsp.h"

#include <iomanip>

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

namespace dsp56k
{
	static bool g_dumpPC = false;
//	static const TWord g_dumpPCictrMin = 0x153000;
	static const TWord g_dumpPCictrMin = 0xfffff;

	// _____________________________________________________________________________
	// DSP
	//
	DSP::DSP( Memory& _memory, IPeripherals* _pX, IPeripherals* _pY  )
		: mem(_memory)
		, perif({_pX, _pY})
		, pcCurrentInstruction(0xffffff)
	{
		mem.setDSP(this);
		perif[0]->setDSP(this);
		perif[1]->setDSP(this);

		m_asm[0] = 0;

		m_opcodeCache.resize(mem.size(), ResolveCache);

		resetHW();
	}

	// _____________________________________________________________________________
	// resetHW
	//
	void DSP::resetHW()
	{
		// 100162AEd01.pdf - page 2-16

		// TODO: internal peripheral devices are reset
		perif[0]->reset();
		if(perif[1] != perif[0])
			perif[1]->reset();

		reg.m[0] = reg.m[1] = reg.m[2] = reg.m[3] = reg.m[4] = reg.m[5] = reg.m[6] = reg.m[7] = TReg24(int(0xffffff));
		reg.r[0] = reg.r[1] = reg.r[2] = reg.r[3] = reg.r[4] = reg.r[5] = reg.r[6] = reg.r[7] = TReg24(int(0));
		reg.n[0] = reg.n[1] = reg.n[2] = reg.n[3] = reg.n[4] = reg.n[5] = reg.n[6] = reg.n[7] = TReg24(int(0));

		iprc(0);
		iprp(0);

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

		perif[0]->exec();

		if(perif[1] != perif[0])
			perif[1]->exec();

		m_currentOpLen = 1;
		
		if(m_processingMode == Default && !m_pendingInterrupts.empty())
		{
			// TODO: priority sorting, masking
			const auto vba = m_pendingInterrupts.pop_front();

			const auto minPrio = mr().var & 0x3;

			const auto op0 = memRead(MemArea_P, vba);
			const auto op1 = memRead(MemArea_P, vba+1);

			const auto oldSP = reg.sp.var;

			m_opWordB = op1;

			m_processingMode = FastInterrupt;

			pcCurrentInstruction = vba;
			getASM(op0, op1);
			execOp(op0);

			auto jumped = reg.sp.var - oldSP;

			// only exec the second op if the first one was a one-word op and we did not jump into a long interrupt
			if(m_currentOpLen == 1 && !jumped)
			{
				pcCurrentInstruction = vba+1;
				getASM(op1, 0);
				execOp(op1);

				m_processingMode = DefaultPreventInterrupt;
			}
			else if(jumped)
			{
				m_processingMode = LongInterrupt;
			}

			return;
		}

		if(m_processingMode == DefaultPreventInterrupt)
			m_processingMode = Default;

		pcCurrentInstruction = reg.pc.toWord();

		const auto op = fetchPC();
		m_opWordB = memRead(MemArea_P, reg.pc.var);

		getASM(op, m_opWordB);

#ifdef _DEBUG
		if( g_dumpPC && pcCurrentInstruction != reg.pc.toWord() && reg.ictr.var >= g_dumpPCictrMin )
		{
			std::stringstream ssout;

			for( int i=0; i<reg.sc.var; ++i )
				ssout << "\t";

			ssout << "EXEC @ " << std::hex << reg.pc.toWord() << " asm = " << m_asm;

			LOGF( ssout.str() );
		}
#endif

		execOp(op);
	}

	void DSP::execOp(const TWord op)
	{
		auto& opCache = m_opcodeCache[pcCurrentInstruction];

		const auto nonParallelOp = opCache & 0xff;
		const auto& oi = Opcodes::getOpcodeInfoAt(nonParallelOp);

		exec_nonParallel(&oi, op);

		const auto parallelOp = (opCache>>8) & 0xff;
		const auto& oiMove = Opcodes::getOpcodeInfoAt(parallelOp);

		exec_parallel(&oiMove, op);

		if( g_dumpPC && reg.ictr.var >= g_dumpPCictrMin )
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
					regChange.pc = pcCurrentInstruction;
					regChange.ictr = reg.ictr.var;

					m_regChanges.push_back( regChange );

					LOGF( "Reg " << g_regNames[i] << " changed @ " << std::hex << regChange.pc << ", ictr " << regChange.ictr << ", old " << std::hex << regChange.valOld.var << ", new " << regChange.valNew.var );

					m_prevRegStates[i].val = regVal;
				}
			}
		}
		++reg.ictr.var;
	}

	bool DSP::exec_parallel(const OpcodeInfo* oi, const TWord op)
	{
		switch (oi->getInstruction())
		{
		case Nop:
			return true;
		case Ifcc:		// execute the specified ALU operation of the condition is true
		case Ifcc_U:
			{
				const auto cccc = getFieldValue<Ifcc_U, Field_CCCC>(op);
				if( !decode_cccc( cccc ) )
					return true;

				const auto backupCCR = ccr();
				const auto res = exec_parallel_alu(op);
				if(oi->getInstruction() == Ifcc)
					ccr(backupCCR);
				return res;
			}
		case Move_Nop:
			if(!(op&0xff))
				return true;
			return exec_parallel_alu(op);
		default:
			if(!(op&0xff))
				return exec_parallel_move(oi, op);

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
#ifdef _DEBUG
		const auto* opInfo = m_opcodes.findParallelAluOpcodeInfo(op);
#endif
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
		case Move_xx:		// (...) #xx,D - Immediate Short Data Move - 001dddddiiiiiiii
			{
				const TWord ddddd = getFieldValue<Move_xx, Field_ddddd>(op);
				const TReg8 iiiiiiii = TReg8(static_cast<uint8_t>(getFieldValue<Move_xx, Field_iiiiiiii>(op)));

				decode_ddddd_write(ddddd, iiiiiiii);
			}
			return true;
		case Mover:
			{
				const auto eeeee = getFieldValue<Mover,Field_eeeee>(op);
				const auto ddddd = getFieldValue<Mover,Field_ddddd>(op);

				// TODO: we need to determine the two register types and call different template versions of read and write
				decode_ddddd_write<TReg24>( ddddd, decode_ddddd_read<TReg24>( eeeee ) );
			}
			return true;
		case Move_ea:						// does not move but updates r[x]
			{
				const TWord mmrrr = getFieldValue<Move_ea, Field_MM, Field_RRR>(op);
				decode_MMMRRR_read( mmrrr );
			}
			return true;
		case Movex_ea:		// (...) X:ea<>D - 01dd0ddd W1MMMRRR			X Memory Data Move
		case Movey_ea:		// (...) Y:ea<>D - 01dd1ddd W1MMMRRR			Y Memory Data Move
			{
				const TWord mmmrrr	= getFieldValue<Movex_ea,Field_MMM, Field_RRR>(op);
				const TWord ddddd	= getFieldValue<Movex_ea,Field_dd, Field_ddd>(op);
				const TWord write	= getFieldValue<Movex_ea,Field_W>(op);

				exec_move_ddddd_MMMRRR( ddddd, mmmrrr, write, oi->getInstruction() == Movex_ea ? MemArea_X : MemArea_Y);
			}
			return true;
		case Movexr_ea:		// X Memory and Register Data Move		(...) X:ea,D1 S2,D2 - 0001ffdF W0MMMRRR
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
			return true;
		case Movexr_A: 
			LOG_ERR_NOTIMPLEMENTED("Movexr_A");
			return true;
		case Movex_aa:   // 01dd0ddd W0aaaaaa ????????
		case Movey_aa:   // 01dd1ddd W0aaaaaa ????????
			{
				const bool write	= getFieldValue<Movex_aa,Field_W>(op);
				const TWord ddddd	= getFieldValue<Movex_aa,Field_dd, Field_ddd>(op);
				const TWord aaaaaa	= getFieldValue<Movex_aa,Field_aaaaaa>(op);
				const EMemArea area	= oi->getInstruction() == Movex_aa ? MemArea_X : MemArea_Y;

				if (write)
				{
					decode_ddddd_write<TReg24>( ddddd, TReg24(memRead(area, aaaaaa)) );
				}
				else
				{
					memWrite( area, aaaaaa, decode_ddddd_read<TWord>(ddddd) );
				}
			}
			return true;
		case Moveyr_ea:			// Register and Y Memory Data Move - (...) S1,D1 Y:ea,D2 - 0001deff W1MMMRRR
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
					const TWord ea = decode_MMMRRR_read(mmmrrr);
					memWrite( MemArea_Y, ea, decode_ff_read( ff ).toWord() );
				}
			}
			return true;
		case Moveyr_A:
			LOG_ERR_NOTIMPLEMENTED("MOVE YR A");
			return true;
		// Long Memory Data Move
		case Movel_ea:				// 0100L0LLW1MMMRRR
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
			return true;
		case Movel_aa:				// 0100L0LLW0aaaaaa
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
			return true;
		case Movexy:					// XY Memory Data Move - 1wmmeeff WrrMMRRR
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
			return true;
		default:
			return false;
		}
	}
	
	inline bool DSP::exec_nonParallel(const OpcodeInfo* oi, TWord op)
	{
		switch (oi->getInstruction())
		{
		// Add
		case Add_xx:	// 0000000101iiiiii10ood000
			{
				const TWord iiiiii	= getFieldValue<Add_xx,Field_iiiiii>(op);
				const auto ab		= getFieldValue<Add_xx,Field_d>(op);

				alu_add( ab, TReg56(iiiiii) );
			}
			return true;
		case Add_xxxx:
			{
				const auto ab = getFieldValue<Add_xxxx,Field_d>(op);

				TReg56 r56;
				convert( r56, TReg24(fetchOpWordB()) );

				alu_add( ab, r56 );
			}
			return true;
		// Logical AND
		case And_xx:	// 0000000101iiiiii10ood110
			{
				const auto ab		= getFieldValue<And_xx,Field_d>(op);
				const TWord xxxx	= getFieldValue<And_xx,Field_iiiiii>(op);

				alu_and(ab, xxxx );
			}
			return true;
		case And_xxxx:
			{
				const auto ab = getFieldValue<And_xxxx,Field_d>(op);
				const TWord xxxx = fetchOpWordB();

				alu_and( ab, xxxx );
			}
			return true;
		// AND Immediate With Control Register
		case Andi:	// AND(I) #xx,D		- 00000000 iiiiiiii 101110EE
			{
				const TWord ee		= getFieldValue<Andi,Field_EE>(op);
				const TWord iiiiii	= getFieldValue<Andi,Field_iiiiiiii>(op);

				TReg8 val = decode_EE_read(ee);
				val.var &= iiiiii;
				decode_EE_write(ee,val);			
			}
			return true;
		// Arithmetic Shift Accumulator Left
		case Asl_ii:	// 00001100 00011101 SiiiiiiD
			{
				const TWord shiftAmount	= getFieldValue<Asl_ii,Field_iiiiii>(op);

				const bool abDst		= getFieldValue<Asl_ii,Field_D>(op);
				const bool abSrc		= getFieldValue<Asl_ii,Field_S>(op);

				alu_asl( abDst, abSrc, shiftAmount );			
			}
			return true;
		case Asl_S1S2D:	// 00001100 00011110 010SsssD
			{
				const TWord sss = getFieldValue<Asl_S1S2D,Field_sss>(op);
				const bool abDst = getFieldValue<Asl_S1S2D,Field_D>(op);
				const bool abSrc = getFieldValue<Asl_S1S2D,Field_S>(op);

				const TWord shiftAmount = decode_sss_read<TWord>( sss );

				alu_asl( abDst, abSrc, shiftAmount );			
			}
			return true;
		// Arithmetic Shift Accumulator Right
		case Asr_ii:	// 00001100 00011100 SiiiiiiD
			{
				const TWord shiftAmount	= getFieldValue<Asr_ii,Field_iiiiii>(op);

				const bool abDst		= getFieldValue<Asr_ii,Field_D>(op);
				const bool abSrc		= getFieldValue<Asr_ii,Field_S>(op);

				alu_asr( abDst, abSrc, shiftAmount );
			}
			return true;
		case Asr_S1S2D:
			{
				const TWord sss = getFieldValue<Asr_S1S2D,Field_sss>(op);
				const bool abDst = getFieldValue<Asr_S1S2D,Field_D>(op);
				const bool abSrc = getFieldValue<Asr_S1S2D,Field_S>(op);

				const TWord shiftAmount = decode_sss_read<TWord>( sss );

				alu_asr( abDst, abSrc, shiftAmount );			
			}
			return true;
		// Branch conditionally
		case Bcc_xxxx:	// TODO: unclear documentation, opcode that is written there is wrong
			LOG_ERR_NOTIMPLEMENTED("BCC xxxx");
			return false;
		case Bcc_xxx:	// 00000101 CCCC01aa aa0aaaaa
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
			return true;
		case Bcc_Rn:		// 00001101 00011RRR 0100CCCC
			LOG_ERR_NOTIMPLEMENTED("Bcc Rn");
			return true;
		// Bit test and change
		case Bchg_ea:
		case Bchg_aa:
		case Bchg_pp:
		case Bchg_qq:
			LOG_ERR_NOTIMPLEMENTED("BCHG");
			return true;
		case Bchg_D:	// 00001011 11DDDDDD 010bbbbb
			{
				const TWord bit		= getFieldValue<Bchg_D,Field_bbbbb>(op);
				const TWord dddddd	= getFieldValue<Bchg_D,Field_DDDDDD>(op);

				TReg24 val = decode_dddddd_read( dddddd );

				sr_toggle( SR_C, bittestandchange( val, bit ) );

				decode_dddddd_write( dddddd, val );

				sr_s_update();
				sr_l_update_by_v();
			}
			return true;
		// Bit test and clear
		case Bclr_ea:
		case Bclr_aa:
		case Bclr_pp:
			LOG_ERR_NOTIMPLEMENTED("BCLR");
			return true;
		case Bclr_qq:	// 0 0 0 0 0 0 0 1 0 0 q q q q q q 0 S 0 b b b b b
			{
				const TWord bit = getFieldValue<Bclr_qq,Field_bbbbb>(op);
				const TWord ea	= getFieldValue<Bclr_qq,Field_qqqqqq>(op);

				const EMemArea S = getFieldValue<Bclr_qq,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord res = alu_bclr( bit, memRead( S, ea ) );

				memWritePeriphFFFF80( S, ea, res );			
			}
			return true;
		case Bclr_D:	// 0000101011DDDDDD010bbbbb
			{
				const TWord bit		= getFieldValue<Bclr_D,Field_bbbbb>(op);
				const TWord dddddd	= getFieldValue<Bclr_D,Field_DDDDDD>(op);

				TWord val;
				convert( val, decode_dddddd_read( dddddd ) );

				const TWord newVal = alu_bclr( bit, val );
				decode_dddddd_write( dddddd, TReg24(newVal) );			
			}
			return true;
		// Branch always
		case Bra_xxxx:
			{
				const int displacement = signextend<int,24>(fetchOpWordB());
				setPC(pcCurrentInstruction + displacement);
			}
			return true;
		case Bra_xxx:		// 00000101 000011aa aa0aaaaa
			{
				const TWord addr = getFieldValue<Bra_xxx,Field_aaaa, Field_aaaaa>(op);

				const int displacement = signextend<int,9>(addr);

				setPC(pcCurrentInstruction + displacement);
			}
			return true;
		case Bra_Rn:		// 0000110100011RRR11000000
			LOG_ERR_NOTIMPLEMENTED("BRA_Rn");
			return true;
		// Branch if Bit Clear
		case Brclr_ea:	// BRCLR #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 0 b b b b b
		case Brclr_aa:	// BRCLR #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 0 b b b b b
			LOG_ERR_NOTIMPLEMENTED("BRCLR");			
			return true;
		case Brclr_pp:	// BRCLR #n,[X or Y]:pp,xxxx - 0 0 0 0 1 1 0 0 1 1 p p p p p p 0 S 0 b b b b b
			{
				const TWord bit		= getFieldValue<Brclr_pp,Field_bbbbb>(op);
				const TWord pppppp	= getFieldValue<Brclr_pp,Field_pppppp>(op);
				const EMemArea S	= getFieldValue<Brclr_pp,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea = pppppp;

				const int displacement = signextend<int,24>(fetchOpWordB());

				if( !bittest( memReadPeriphFFFFC0( S, ea ), bit ) )
				{
					setPC(pcCurrentInstruction + displacement);
				}
			}
			return true;
		case Brclr_qq:
			{
				const TWord bit		= getFieldValue<Brclr_qq,Field_bbbbb>(op);
				const TWord qqqqqq	= getFieldValue<Brclr_qq,Field_qqqqqq>(op);
				const EMemArea S	= getFieldValue<Brclr_qq,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea = qqqqqq;

				const int displacement = signextend<int,24>(fetchOpWordB());

				if( !bittest( memReadPeriphFFFF80( S, ea ), bit ) )
				{
					setPC(pcCurrentInstruction + displacement);
				}
			}
			return true;
		case Brclr_S:	// BRCLR #n,S,xxxx - 0 0 0 0 1 1 0 0 1 1 D D D D D D 1 0 0 b b b b b
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
			return true;
		case BRKcc:					// Exit Current DO Loop Conditionally
			LOG_ERR_NOTIMPLEMENTED("BRKcc");
			return true;
		// Branch if Bit Set
		case Brset_ea:	// BRSET #n,[X or Y]:ea,xxxx - 0 0 0 0 1 1 0 0 1 0 M M M R R R 0 S 1 b b b b b
		case Brset_aa:	// BRSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 1 0 0 1 0 a a a a a a 1 S 1 b b b b b
		case Brset_pp:	// BRSET #n,[X or Y]:pp,xxxx - 
			LOG_ERR_NOTIMPLEMENTED("BRSET");
			return true;
		case Brset_qq:
			{
				const TWord bit		= getFieldValue<Brset_qq,Field_bbbbb>(op);
				const TWord qqqqqq	= getFieldValue<Brset_qq,Field_qqqqqq>(op);
				const EMemArea S	= getFieldValue<Brset_qq,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea = qqqqqq;

				const int displacement = signextend<int,24>( fetchOpWordB() );

				if( bittest( memReadPeriphFFFF80( S, ea ), bit ) )
				{
					setPC(pcCurrentInstruction + displacement);
				}
			}
			return true;
		case Brset_S:
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
			return true;
		// Branch to Subroutine Conditionally
		case BScc_xxxx:		// 00001101000100000000CCCC
			{
				const TWord cccc = getFieldValue<BScc_xxxx,Field_CCCC>(op);

				const int displacement = signextend<int,24>(fetchOpWordB());

				if( decode_cccc(cccc) )
				{
					jsr(pcCurrentInstruction + displacement);
				}
			}
			return true;
		case BScc_xxx:		// 00000101CCCC00aaaa0aaaaa
			{
				const TWord cccc = getFieldValue<BScc_xxx,Field_CCCC>(op);

				if( decode_cccc(cccc) )
				{
					const TWord addr = getFieldValue<BScc_xxx,Field_aaaa, Field_aaaaa>(op);

					const int displacement = signextend<int,9>(addr);

					jsr(pcCurrentInstruction + displacement);
				}				
			}
			return true;
		case BScc_Rn:
			LOG_ERR_NOTIMPLEMENTED("BScc Rn");
			return true;
		// Branch to Subroutine if Bit Clear
		case Bsclr_ea:
		case Bsclr_aa:
		case Bsclr_qq:
		case Bsclr_pp:
		case Bsclr_S:
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
			return true;
		case Bset_ea:	// 0000101001MMMRRR0S1bbbbb
			// Bit Set and Test
			{
				const TWord bit		= getFieldValue<Bset_ea,Field_bbbbb>(op);
				const TWord mmmrrr	= getFieldValue<Bset_ea,Field_MMM, Field_RRR>(op);
				const EMemArea S	= getFieldValue<Bset_ea,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea		= decode_MMMRRR_read(mmmrrr);

				TWord val = memRead( S, ea );

				sr_toggle( SR_C, bittestandset( val, bit ) );

				memWrite( S, ea, val );
			}
			return true;
		case Bset_aa:
		case Bset_pp:
			LOG_ERR_NOTIMPLEMENTED("BSET");
			return true;
		case Bset_qq:
			{
				const TWord bit		= getFieldValue<Bset_qq,Field_bbbbb>(op);
				const TWord qqqqqq	= getFieldValue<Bset_qq,Field_qqqqqq>(op);
				const EMemArea S	= getFieldValue<Bset_qq,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea		= qqqqqq;

				TWord val = memReadPeriphFFFF80( S, ea );

				sr_toggle( SR_C, bittestandset( val, bit ) );

				memWritePeriphFFFF80( S, ea, val );
			}
			return true;
		case Bset_D:	// 0000101011DDDDDD011bbbbb
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
			return true;
		// Branch to Subroutine
		case Bsr_xxxx:
			{
				const int displacement = signextend<int,24>(fetchOpWordB());
				jsr(pcCurrentInstruction + displacement);
			}
			return true;
		case Bsr_xxx:		// 00000101000010aaaa0aaaaa
			{
				const TWord aaaaaaaaa = getFieldValue<Bsr_xxx,Field_aaaa, Field_aaaaa>(op);

				const int displacement = signextend<int,9>(aaaaaaaaa);

				jsr(pcCurrentInstruction + displacement);
			}
			return true;
		case Bsr_Rn:  // 0000110100011RRR10000000
            {
                const auto rrr = getFieldValue<Bsr_Rn,Field_RRR>(op);
                jsr(pcCurrentInstruction + reg.r[rrr].var);
            }
            return true;
		// Branch to Subroutine if Bit Set
		case Bsset_ea:
		case Bsset_aa:
		case Bsset_qq:
		case Bsset_pp:
		case Bsset_S:
			LOG_ERR_NOTIMPLEMENTED("BSCLR");
			return true;
		// Bit Test
		case Btst_ea:
			{
				const TWord bit = getFieldValue<Btst_ea,Field_bbbbb>(op);
				const TWord mmmrrr	= getFieldValue<Btst_ea,Field_MMM, Field_RRR>(op);
				const TWord ea = decode_MMMRRR_read( mmmrrr );
				const EMemArea S = getFieldValue<Btst_ea,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord val = memRead( S, ea );

				sr_toggle( SR_C, bittest( val, bit ) );

				sr_s_update();
				sr_l_update_by_v();
			}
			return true;
		case Btst_aa:
			LOG_ERR_NOTIMPLEMENTED("BTST aa");
			return true;
		case Btst_pp:	// 0 0 0 0 1 0 1 1 1 0 p p p p p p 0 S 1 b b b b b
			{
				const TWord bitNum	= getFieldValue<Btst_pp,Field_bbbbb>(op);
				const TWord pppppp	= getFieldValue<Btst_pp,Field_pppppp>(op);
				const EMemArea S	= getFieldValue<Btst_pp,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord memVal	= memReadPeriphFFFFC0( S, pppppp );

				const bool bitSet	= ( memVal & (1<<bitNum)) != 0;

				sr_toggle( SR_C, bitSet );
			}
			return true;
		case Btst_qq:	// 0 0 0 0 0 0 0 1 0 1 q q q q q q 0 S 1 b b b b b
			LOG_ERR_NOTIMPLEMENTED("BTST qq");
			return true;
		case Btst_D:	// 0000101111DDDDDD011bbbbb
			{
				const TWord dddddd	= getFieldValue<Btst_D,Field_DDDDDD>(op);
				const TWord bit		= getFieldValue<Btst_D,Field_bbbbb>(op);

				TReg24 val = decode_dddddd_read( dddddd );

				sr_toggle( SR_C, bittest( val.var, bit ) );
			}
			return true;
		case Clb:					// Count Leading Bits - // CLB S,D - 0 0 0 0 1 1 0 0 0 0 0 1 1 1 1 0 0 0 0 0 0 0 S D
			LOG_ERR_NOTIMPLEMENTED("CLB");
			return true;
		// Compare
		case Cmp_xxS2:				// CMP #xx, S2 - 00000001 01iiiiii 1000d101
			{
				const TWord iiiiii = getFieldValue<Cmp_xxS2,Field_iiiiii>(op);
				
				TReg56 r56;
				convert( r56, TReg24(iiiiii) );

				alu_cmp( bittest(op,3), r56, false );				
			}
			return true;
		case Cmp_xxxxS2:
			{
				const TReg24 s( signextend<int,24>( fetchOpWordB() ) );

				TReg56 r56;
				convert( r56, s );

				alu_cmp( bittest(op,3), r56, false );				
			}
			return true;
		// Compare Unsigned
		case Cmpu_S1S2:
			LOG_ERR_NOTIMPLEMENTED("CMPU");
			return true;
		case Debug:
			LOG( "Entering DEBUG mode" );
			LOG_ERR_NOTIMPLEMENTED("DEBUG");
			return true;
		case Debugcc:
			{
				const TWord cccc = getFieldValue<Debugcc,Field_CCCC>(op);
				if( decode_cccc( cccc ) )
				{
					LOG( "Entering DEBUG mode because condition is met" );
					LOG_ERR_NOTIMPLEMENTED("DEBUGcc");
				}
			}
			return true;
		case Dec:			// Decrement by One
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
			return true;
		case Div:	// 0000000110oooooo01JJdooo
			{
				// TODO: i'm not sure if this works as expected...

				const TWord jj	= getFieldValue<Div,Field_JJ>(op);
				const auto ab	= getFieldValue<Div,Field_d>(op);

				TReg56& d = ab ? reg.b : reg.a;

				TReg24 s24 = decode_JJ_read( jj );

				const TReg56 debugOldD = d;
				const TReg24 debugOldS = s24;

				bool c = bittest(d,55) != bittest(s24,23);

				bool old47 = bittest(d,47);

				d.var <<= 1;

				bool changed47 = (bittest( d, 47 ) != 0) != c;

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
		case Dmac:	// 000000010010010s1SdkQQQQ
			{
				const bool dstUnsigned	= getFieldValue<Dmac,Field_s>(op);
				const bool srcUnsigned	= getFieldValue<Dmac,Field_S>(op);
				const bool ab			= getFieldValue<Dmac,Field_d>(op);
				const bool negate		= getFieldValue<Dmac,Field_k>(op);

				const TWord qqqq		= getFieldValue<Dmac,Field_QQQQ>(op);

				TReg24 s1, s2;
				decode_QQQQ_read( s1, s2, qqqq );

				// TODO: untested
				alu_dmac( ab, s1, s2, negate, srcUnsigned, dstUnsigned );
			}
			return true;
		// Start Hardware Loop
		case Do_ea:
		case Do_aa:
			LOG_ERR_NOTIMPLEMENTED("DO");
			return true;
		case Do_xxx:	// 00000110iiiiiiii1000hhhh
			{
				const TWord addr = fetchOpWordB();
				TWord loopcount = getFieldValue<Do_xxx,Field_hhhh, Field_iiiiiiii>(op);

				do_start( TReg24(loopcount), addr );				
			}
			return true;
		case Do_S:		// 0000011011DDDDDD00000000
			{
				const TWord addr = fetchOpWordB();

				const TWord dddddd = getFieldValue<Do_S,Field_DDDDDD>(op);

				const TReg24 loopcount = decode_dddddd_read( dddddd );

				do_start( loopcount, addr );
			}
			return true;
		case DoForever:
			LOG_ERR_NOTIMPLEMENTED("DO FOREVER");
			return true;
		// Start Infinite Loop
		case Dor_ea:	// 0000011001MMMRRR0S010000
		case Dor_aa:	// 0000011000aaaaaa0S010000
			LOG_ERR_NOTIMPLEMENTED("DOR");
			return true;
		case Dor_xxx:	// 00000110iiiiiiii1001hhhh
            {
	            const auto loopcount = getFieldValue<Dor_xxx,Field_hhhh, Field_iiiiiiii>(op);
                const auto displacement = signextend<int, 24>(fetchOpWordB());
                do_start(TReg24(loopcount), pcCurrentInstruction + displacement);
            }
            return true;
		case Dor_S:		// 00000110 11DDDDDD 00010000
			{
				const TWord dddddd = getFieldValue<Dor_S,Field_DDDDDD>(op);
				const TReg24 lc		= decode_dddddd_read( dddddd );
				
				const int displacement = signextend<int,24>(fetchOpWordB());
				do_start( lc, pcCurrentInstruction + displacement);
			}
			return true;
		// Start PC-Relative Infinite Loops
		case DorForever:
			LOG_ERR_NOTIMPLEMENTED("DOR FOREVER");
			return true;
		case Enddo:			// End Current DO Loop
			// restore previous loop flag
			sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

			// decrement SP twice, restoring old loop settings
			decSP();

			reg.lc = ssl();
			reg.la = ssh();

			return true;
		case Eor_xx:				// Logical Exclusive OR
		case Eor_xxxx:
			LOG_ERR_NOTIMPLEMENTED("EOR");
			return true;
		case Extract_S1S2:			// Extract Bit Field
		case Extract_CoS2:
			LOG_ERR_NOTIMPLEMENTED("EXTRACT");
			return true;
		case Extractu_S1S2:			// Extract Unsigned Bit Field
		case Extractu_CoS2:
			LOG_ERR_NOTIMPLEMENTED("EXTRACTU");
			return true;
		case Illegal:
			LOG_ERR_NOTIMPLEMENTED("ILLEGAL");
			return true;
		case Inc:			// Increment by One	
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
			return true;
		case Insert_S1S2:			// Insert Bit Field
		case Insert_CoS2:
			LOG_ERR_NOTIMPLEMENTED("INSERT");
			return true;
		// Jump Conditionally
		case Jcc_xxx:		// 00001110CCCCaaaaaaaaaaaa
			{
				const TWord cccc = getFieldValue<Jcc_xxx,Field_CCCC>(op);

				if( decode_cccc( cccc ) )
				{
					const TWord ea = getFieldValue<Jcc_xxx,Field_aaaaaaaaaaaa>(op);
					setPC(ea);
				}				
			}
			return true;
		case Jcc_ea:
			{
				const TWord cccc	= getFieldValue<Jcc_ea,Field_CCCC>(op);
				const TWord mmmrrr	= getFieldValue<Jcc_ea,Field_MMM, Field_RRR>(op);

				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				if( decode_cccc( cccc ) )
				{
					setPC(ea);
				}
			}
			return true;
		// Jump if Bit Clear
		case Jclr_ea:	// 0000101001MMMRRR1S0bbbbb
			{
				const TWord bit		= getFieldValue<Jclr_ea,Field_bbbbb>(op);
				const TWord mmmrrr	= getFieldValue<Jclr_ea,Field_MMM, Field_RRR>(op);
				const EMemArea S	= getFieldValue<Jclr_ea,Field_S>(op) ? MemArea_Y : MemArea_X;
				const TWord addr	= fetchOpWordB();

				const TWord ea		= decode_MMMRRR_read(mmmrrr);

				if( !bittest( ea, bit ) )	// TODO: S is not used, need to read mem if mmmrrr is not immediate data!
				{
					setPC(addr);
				}

				LOG_ERR_NOTIMPLEMENTED("JCLR");
			}
			return true;
		case Jclr_aa:
			LOG_ERR_NOTIMPLEMENTED("JCLR aa");
			return true;
		case Jclr_pp:
			LOG_ERR_NOTIMPLEMENTED("JCLR pp");
			return true;
		case Jclr_qq:	// 00000001 10qqqqqq 1S0bbbbb
			{
				const TWord qqqqqq	= getFieldValue<Jclr_qq,Field_qqqqqq>(op);
				const TWord bit		= getFieldValue<Jclr_qq,Field_bbbbb>(op);
				const EMemArea S	= getFieldValue<Jclr_qq,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea		= qqqqqq;

				const TWord addr	= fetchOpWordB();

				if( !bittest( memReadPeriphFFFF80(S, ea), bit ) )
					setPC(addr);
			}
			return true;
		case Jclr_S:	// 00001010 11DDDDDD 000bbbbb
			{
				const TWord dddddd	= getFieldValue<Jclr_S,Field_DDDDDD>(op);
				const TWord bit		= getFieldValue<Jclr_S,Field_bbbbb>(op);

				const TWord addr = fetchOpWordB();

				if( !bittest( decode_dddddd_read(dddddd), bit ) )
					setPC(addr);
			}
			return true;
		// Jump
		case Jmp_ea:
			{
				const TWord mmmrrr	= getFieldValue<Jmp_ea,Field_MMM, Field_RRR>(op);
				setPC(decode_MMMRRR_read(mmmrrr));
			}
			return true;
		case Jmp_xxx:	// 00001100 0000aaaa aaaaaaaa
			setPC(getFieldValue<Jmp_xxx,Field_aaaaaaaaaaaa>(op));
			return true;
		// Jump to Subroutine Conditionally
		case Jscc_xxx:
			{
				const TWord cccc	= getFieldValue<Jscc_xxx,Field_CCCC>(op);

				if( decode_cccc( cccc ) )
				{
					const TWord a = getFieldValue<Jscc_xxx,Field_aaaaaaaaaaaa>(op);
					jsr(a);
				}
			}
			return true;
		case Jscc_ea:
			{
				const TWord cccc	= getFieldValue<Jscc_ea,Field_CCCC>(op);
				const TWord mmmrrr	= getFieldValue<Jscc_ea,Field_MMM, Field_RRR>(op);
				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				if( decode_cccc( cccc ) )
				{
					jsr(ea);
				}
			}
			return true;
		case Jsclr_ea:
		case Jsclr_aa:
		case Jsclr_pp:
		case Jsclr_qq:
		case Jsclr_S:
			LOG_ERR_NOTIMPLEMENTED("JSCLR");
			return true;
		// Jump if Bit Set
		case Jset_ea:	// 0000101001MMMRRR1S1bbbbb
			{
				const TWord bit		= getFieldValue<Jset_ea,Field_bbbbb>(op);
				const TWord mmmrrr	= getFieldValue<Jset_ea,Field_MMM, Field_RRR>(op);
				const EMemArea S	= getFieldValue<Jset_ea,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord val		= memRead( S, decode_MMMRRR_read( mmmrrr ) );

				if( bittest(val,bit) )
				{
					setPC(val);
				}
			}
			return true;
		case Jset_aa:	// JSET #n,[X or Y]:aa,xxxx - 0 0 0 0 1 0 1 0 0 0 a a a a a a 1 S 1 b b b b b
			LOG_ERR_NOTIMPLEMENTED("JSET #n,[X or Y]:aa,xxxx");
			return true;
		case Jset_pp:	// JSET #n,[X or Y]:pp,xxxx - 0 0 0 0 1 0 1 0 1 0 p p p p p p 1 S 1 b b b b b
			{
				const TWord pppppp	= getFieldValue<Jset_pp,Field_pppppp>(op);
				const TWord bit		= getFieldValue<Jset_pp,Field_bbbbb>(op);
				const EMemArea S	= getFieldValue<Jset_pp,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea		= pppppp;

				const TWord addr	= fetchOpWordB();

				if( bittest( memReadPeriphFFFFC0(S, ea), bit ) )
					setPC(addr);
			}
			return true;
		case Jset_qq:
			{
				// TODO: combine code with Jset_pp, only the offset is different
				const TWord qqqqqq	= getFieldValue<Jset_qq,Field_qqqqqq>(op);
				const TWord bit		= getFieldValue<Jset_qq,Field_bbbbb>(op);
				const EMemArea S	= getFieldValue<Jset_qq,Field_S>(op) ? MemArea_Y : MemArea_X;

				const TWord ea		= qqqqqq;

				const TWord addr	= fetchOpWordB();

				if( bittest( memReadPeriphFFFF80(S, ea), bit ) )
					setPC(addr);
			}
			return true;
		case Jset_S:	// 0000101011DDDDDD001bbbbb
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
			return true;
		// Jump to Subroutine
		case Jsr_ea:
			{
				const TWord mmmrrr = getFieldValue<Jsr_ea,Field_MMM, Field_RRR>(op);

				const TWord ea = decode_MMMRRR_read( mmmrrr );

				jsr(TReg24(ea));
			}
			return true;
		case Jsr_xxx:
			{
				const TWord ea = getFieldValue<Jsr_xxx,Field_aaaaaaaaaaaa>(op);
				jsr(TReg24(ea));
			}
			return true;
		// Jump to Subroutine if Bit Set
		case Jsset_ea:
		case Jsset_aa:
		case Jsset_pp:
		case Jsset_qq:
		case Jsset_S:
			LOG_ERR_NOTIMPLEMENTED("JSSET");
			return true;
		// Load PC-Relative Address
		case Lra_Rn:
		case Lra_xxxx:
			LOG_ERR_NOTIMPLEMENTED("LRA");
			return true;
		case Lsl_ii:				// Logical Shift Left		000011000001111010iiiiiD
			{
	            const auto shiftAmount = getFieldValue<Lsl_ii,Field_iiiii>(op);
	            const auto abDst = getFieldValue<Lsl_ii,Field_D>(op);

	            alu_lsl(abDst, shiftAmount);
	        }
			return true;
		case Lsl_SD:
			LOG_ERR_NOTIMPLEMENTED("LSL");
			return true;
		case Lsr_ii:				// Logical Shift Right		000011000001111011iiiiiD
			{
	            const auto shiftAmount = getFieldValue<Lsr_ii,Field_iiiii>(op);
	            const auto abDst = getFieldValue<Lsr_ii,Field_D>(op);

	            alu_lsr(abDst, shiftAmount);
	        }
			return true;
		case Lsr_SD:
			LOG_ERR_NOTIMPLEMENTED("LSR");
			return true;
		// Load Updated Address
		case Lua_ea:	// 00000100010MMRRR000ddddd
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
			return true;
		case Lua_Rn:	// 00000100 00aaaRRR aaaadddd
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
			return true;
		// Signed Multiply Accumulate
		case Mac_S:	// 00000001 000sssss 11QQdk10
			{
				const TWord sssss	= getFieldValue<Mac_S,Field_sssss>(op);
				const TWord qq		= getFieldValue<Mac_S,Field_QQ>(op);
				const bool	ab		= getFieldValue<Mac_S,Field_d>(op);
				const bool	negate	= getFieldValue<Mac_S,Field_k>(op);

				const TReg24 s1 = decode_QQ_read( qq );
				const TReg24 s2( decode_sssss(sssss) );

				alu_mac( ab, s1, s2, negate, false );
			}
			return true;
		// Signed Multiply Accumulate With Immediate Operand
		case Maci_xxxx:
			LOG_ERR_NOTIMPLEMENTED("MACI");
			return true;
		// Mixed Multiply Accumulate
		case Macsu:	// 0 0 0 0 0 0 0 1 0 0 1 0 0 1 1 0 1 s d k Q Q Q Q
			{
				const bool uu			= getFieldValue<Macsu,Field_s>(op);
				const bool ab			= getFieldValue<Macsu,Field_d>(op);
				const bool negate		= getFieldValue<Macsu,Field_k>(op);
				const TWord qqqq		= getFieldValue<Macsu,Field_QQQQ>(op);

				TReg24 s1, s2;
				decode_QQQQ_read( s1, s2, qqqq );

				alu_mac( ab, s1, s2, negate, uu );
			}
			return true;
		// Signed Multiply Accumulate and Round
		case Macr_S:
			LOG_ERR_NOTIMPLEMENTED("MACR");
			return true;
		// Signed MAC and Round With Immediate Operand
		case Macri_xxxx:
			LOG_ERR_NOTIMPLEMENTED("MACRI");
			return true;
		case Merge:					// Merge Two Half Words
			LOG_ERR_NOTIMPLEMENTED("MERGE");
			return true;
		// X or Y Memory Data Move with immediate displacement
		case Movex_Rnxxxx:	// 0000101001110RRR1WDDDDDD
		case Movey_Rnxxxx:	// 0000101101110RRR1WDDDDDD
			{
				const TWord DDDDDD	= getFieldValue<Movex_Rnxxxx,Field_DDDDDD>(op);
				const auto	write	= getFieldValue<Movex_Rnxxxx,Field_W>(op);
				const TWord rrr		= getFieldValue<Movex_Rnxxxx,Field_RRR>(op);

				const int shortDisplacement = signextend<int,24>(fetchOpWordB());
				const TWord ea = decode_RRR_read( rrr, shortDisplacement );

				const auto area = oi->getInstruction() == Movey_Rnxxxx ? MemArea_Y : MemArea_X;

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
		case Movex_Rnxxx:	// 0000001aaaaaaRRR1a0WDDDD
		case Movey_Rnxxx:	// 0000001aaaaaaRRR1a1WDDDD
			{
				const TWord ddddd	= getFieldValue<Movex_Rnxxx,Field_DDDD>(op);
				const TWord aaaaaaa	= getFieldValue<Movex_Rnxxx,Field_aaaaaa, Field_a>(op);
				const auto	write	= getFieldValue<Movex_Rnxxx,Field_W>(op);
				const TWord rrr		= getFieldValue<Movex_Rnxxx,Field_RRR>(op);

				const int shortDisplacement = signextend<int,7>(aaaaaaa);
				const TWord ea = decode_RRR_read( rrr, shortDisplacement );

				const auto area = oi->getInstruction() == Movey_Rnxxx ? MemArea_Y : MemArea_X;

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
		case Movec_ea:		// 00000101W1MMMRRR0S1DDDDD
			{
				const TWord ddddd	= getFieldValue<Movec_ea,Field_DDDDD>(op);
				const TWord mmmrrr	= getFieldValue<Movec_ea,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movec_ea,Field_W>(op);

				const TWord addr = decode_MMMRRR_read( mmmrrr );

				const EMemArea area = getFieldValue<Movec_ea,Field_S>(op) ? MemArea_Y : MemArea_X;
					
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
			return true;
		case Movec_aa:		// 00000101W0aaaaaa0S1DDDDD
			{
				const TWord ddddd	= getFieldValue<Movec_aa,Field_DDDDD>(op);
				const TWord aaaaaa	= getFieldValue<Movec_aa,Field_aaaaaa>(op);
				const auto write	= getFieldValue<Movec_aa,Field_W>(op);

				const TWord addr = aaaaaa;

				const EMemArea area = getFieldValue<Movec_aa,Field_S>(op) ? MemArea_Y : MemArea_X;
					
				if( write )
				{
					decode_ddddd_pcr_write( ddddd, TReg24(memRead( area, addr )) );
				}
				else
				{
					memWrite( area, addr, decode_ddddd_pcr_read(ddddd).toWord() );
				}
			}
			return true;
		case Movec_S1D2:	// 00000100W1eeeeee101ddddd
			{
				const auto write = getFieldValue<Movec_S1D2,Field_W>(op);

				const TWord eeeeee	= getFieldValue<Movec_S1D2,Field_eeeeee>(op);
				const TWord ddddd	= getFieldValue<Movec_S1D2,Field_DDDDD>(op);

				if( write )
					decode_ddddd_pcr_write( ddddd, decode_dddddd_read( eeeeee ) );
				else
					decode_dddddd_write( eeeeee, decode_ddddd_pcr_read( ddddd ) );				
			}
			return true;
		case Movec_xx:		// 00000101iiiiiiii101ddddd
			{
				const TWord iiiiiiii	= getFieldValue<Movec_xx, Field_iiiiiiii>(op);
				const TWord ddddd		= getFieldValue<Movec_xx,Field_DDDDD>(op);
				decode_ddddd_pcr_write( ddddd, TReg24(iiiiiiii) );
			}
			return true;
		// Move Program Memory
		case Movem_ea:		// 00000111W1MMMRRR10dddddd
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
			return true;
		case Movem_aa:		// 00000111W0aaaaaa00dddddd
			LOG_ERR_NOTIMPLEMENTED("MOVE(M) S,P:aa");
			return true;
		// Move Peripheral Data
		case Movep_ppea:	// 0000100sW1MMMRRR1Spppppp
			{
				const TWord pp		= getFieldValue<Movep_ppea,Field_pppppp>(op);
				const TWord mmmrrr	= getFieldValue<Movep_ppea,Field_MMM, Field_RRR>(op);
				const auto write	= getFieldValue<Movep_ppea,Field_W>(op);
				const EMemArea s	= getFieldValue<Movep_ppea,Field_s>(op) ? MemArea_Y : MemArea_X;
				const EMemArea S	= getFieldValue<Movep_ppea,Field_S>(op) ? MemArea_Y : MemArea_X;

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
			return true;
		case Movep_Xqqea:	// 00000111W1MMMRRR0Sqqqqqq
		case Movep_Yqqea:	// 00000111W0MMMRRR1Sqqqqqq
			{
				const TWord mmmrrr	= getFieldValue<Movep_Xqqea,Field_MMM, Field_RRR>(op);
				const EMemArea S	= getFieldValue<Movep_Xqqea,Field_S>(op) ? MemArea_Y : MemArea_X;
				const TWord qAddr	= getFieldValue<Movep_Xqqea,Field_qqqqqq>(op);
				const auto write	= getFieldValue<Movep_Xqqea,Field_W>(op);

				const TWord ea		= decode_MMMRRR_read( mmmrrr );

				const auto area = oi->getInstruction() == Movep_Yqqea ? MemArea_Y : MemArea_X;

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
			return true;
		case Movep_eapp:	// 0000100sW1MMMRRR01pppppp
			LOG_ERR_NOTIMPLEMENTED("MOVE");
			return true;

		case Movep_eaqq:	// 000000001WMMMRRR0Sqqqqqq
			LOG_ERR_NOTIMPLEMENTED("MOVE");
			return true;
		case Movep_Spp:		// 0000100sW1dddddd00pppppp
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
			return true;
		case Movep_SXqq:	// 00000100W1dddddd1q0qqqqq
		case Movep_SYqq:	// 00000100W1dddddd0q1qqqqq
			{
				
				const TWord addr	= getFieldValue<Movep_SXqq,Field_q, Field_qqqqq>(op);
				const TWord dddddd	= getFieldValue<Movep_SXqq,Field_dddddd>(op);
				const auto	write	= getFieldValue<Movep_SXqq,Field_W>(op);

				const auto area = oi->getInstruction() == Movep_SYqq ? MemArea_Y : MemArea_X;

				if( write )
					memWritePeriphFFFF80( area, addr, decode_dddddd_read( dddddd ).toWord() );
				else
					decode_dddddd_write( dddddd, TReg24(memReadPeriphFFFF80( area, addr )) );
			}
			return true;
		// Signed Multiply
		case Mpy_SD:	// 00000001000sssss11QQdk00
			{
				const int sssss		= getFieldValue<Mpy_SD,Field_sssss>(op);
				const TWord QQ		= getFieldValue<Mpy_SD,Field_QQ>(op);
				const bool ab		= getFieldValue<Mpy_SD,Field_d>(op);
				const bool negate	= getFieldValue<Mpy_SD,Field_k>(op);

				const TReg24 s1 = decode_QQ_read(QQ);

				const TReg24 s2 = TReg24( decode_sssss(sssss) );

				alu_mpy( ab, s1, s2, negate, false );
			}
			return true;
		// Mixed Multiply
		case Mpy_su:	// 00000001001001111sdkQQQQ
			{
				const bool ab		= getFieldValue<Mpy_su,Field_d>(op);
				const bool negate	= getFieldValue<Mpy_su,Field_k>(op);
				const bool uu		= getFieldValue<Mpy_su,Field_s>(op);
				const TWord qqqq	= getFieldValue<Mpy_su,Field_QQQQ>(op);

				TReg24 s1, s2;
				decode_QQQQ_read( s1, s2, qqqq );

				alu_mpysuuu( ab, s1, s2, negate, false, uu );
			}
			return true;
		// Signed Multiply With Immediate Operand
		case Mpyi:		// 00000001 01000001 11qqdk00
			{
				const bool	ab		= getFieldValue<Mpyi,Field_d>(op);
				const bool	negate	= getFieldValue<Mpyi,Field_k>(op);
				const TWord qq		= getFieldValue<Mpyi,Field_qq>(op);

				const TReg24 s		= TReg24(fetchOpWordB());

				const TReg24 reg	= decode_qq_read(qq);

				alu_mpy( ab, reg, s, negate, false );
			}
			return true;
		// Signed Multiply and Round
		case Mpyr_SD:
			LOG_ERR_NOTIMPLEMENTED("MPYR");
			return true;
		// Signed Multiply and Round With Immediate Operand
		case Mpyri:
			LOG_ERR_NOTIMPLEMENTED("MPYRI");
			return true;
		case Nop:
			return true;
		// Norm Accumulator Iterations
		case Norm:
			LOG_ERR_NOTIMPLEMENTED("NORM");
			return true;
		// Fast Accumulator Normalization
		case Normf:
			LOG_ERR_NOTIMPLEMENTED("NORMF");
			return true;
		case Or_xx:					// Logical Inclusive OR
		case Or_xxxx:
			LOG_ERR_NOTIMPLEMENTED("OR");
			return true;
		case Ori:					// OR Immediate With Control Register - 00000000iiiiiiii111110EE
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
			return true;
		case Pflush:
			LOG_ERR_NOTIMPLEMENTED("PFLUSH");
			return true;
		case Pflushun:		// Program Cache Flush Unlocked Sections
			cache.pflushun();
			return true;
		case Pfree:			// Program Cache Global Unlock
			cache.pfree();
			return true;
		// Lock Instruction Cache Sector
		case Plock:
			cache.plock(fetchOpWordB());
			return true;
		case Plockr:
			LOG_ERR_NOTIMPLEMENTED("PLOCKR");
			return true;
		case Punlock:
			LOG_ERR_NOTIMPLEMENTED("PUNLOCK");
			return true;
		case Punlockr:
			LOG_ERR_NOTIMPLEMENTED("PUNLOCKR");
			return true;
		// Repeat Next Instruction
		case Rep_ea:
		case Rep_aa:
			LOG_ERR_NOTIMPLEMENTED("REP");
			return true;
		case Rep_xxx:	// 00000110 iiiiiiii 1010hhhh
			{
				const TWord loopcount = getFieldValue<Rep_xxx,Field_hhhh, Field_iiiiiiii>(op);
				rep_exec(loopcount);
			}
			return true;
		case Rep_S:
			LOG_ERR_NOTIMPLEMENTED("REP S");
			return true;
		// Reset On-Chip Peripheral Devices
		case Reset:
			resetSW();
			return true;
		case Rti:			// Return From Interrupt
			popPCSR();
			m_processingMode = DefaultPreventInterrupt;
			return true;
		case Rts:			// Return From Subroutine
			popPC();
			return true;
		case Stop:
			LOG_ERR_NOTIMPLEMENTED("STOP");
			return true;
		// Subtract
		case Sub_xx:
			{
				const auto ab		= getFieldValue<Sub_xx,Field_d>(op);
				const TWord iiiiii	= getFieldValue<Sub_xx,Field_iiiiii>(op);

				alu_sub( ab, TReg56(iiiiii) );
			}
			return true;
		case Sub_xxxx:	// 0000000101ooooo011ood100
			{
				const auto ab = getFieldValue<Sub_xxxx,Field_d>(op);

				TReg56 r56;
				convert( r56, TReg24(fetchOpWordB()) );

				alu_sub( ab, r56 );
			}
			return true;
		// Transfer Conditionally
		case Tcc_S1D1:	// Tcc S1,D1 - 00000010 CCCC0000 0JJJd000
			{
				const TWord cccc = getFieldValue<Tcc_S1D1,Field_CCCC>(op);

				if( decode_cccc( cccc ) )
				{
					const TWord JJJ = getFieldValue<Tcc_S1D1,Field_JJJ>(op);
					const bool ab = getFieldValue<Tcc_S1D1,Field_d>(op);

					decode_JJJ_readwrite( ab ? reg.b : reg.a, JJJ, !ab );
				}
			}
			return true;
		case Tcc_S1D1S2D2:	// Tcc S1,D1 S2,D2 - 00000011 CCCC0ttt 0JJJdTTT
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
			return true;
		case Tcc_S2D2:	// Tcc S2,D2 - 00000010 CCCC1ttt 00000TTT
			{
				const TWord cccc = getFieldValue<Tcc_S2D2,Field_CCCC>(op);

				if( decode_cccc( cccc ) )
				{
					const TWord TTT		= getFieldValue<Tcc_S2D2,Field_TTT>(op);
					const TWord ttt		= getFieldValue<Tcc_S2D2,Field_ttt>(op);
					reg.r[TTT] = reg.r[ttt];
				}
			}
			return true;
		case Trap:
			LOG_ERR_NOTIMPLEMENTED("TRAP");
			return true;
		case Trapcc:
			LOG_ERR_NOTIMPLEMENTED("TRAPcc");
			return true;
		// Viterbi Shift Left
		case Vsl:	// VSL S,i,L:ea - 0 0 0 0 1 0 1 S 1 1 M M M R R R 1 1 0 i 0 0 0 0
			LOG_ERR_NOTIMPLEMENTED("VSL");
			return true;
		case Wait:
			LOG_ERR_NOTIMPLEMENTED("WAIT");
			return true;
		case ResolveCache:
			{
				auto& cacheEntry = m_opcodeCache[pcCurrentInstruction];
				cacheEntry = 0;

				if( op )
				{
					if(Opcodes::isNonParallelOpcode(op))
					{
						const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);

						if(!oi)
						{
							m_opcodes.findNonParallelOpcodeInfo(op);	// retry here to help debugging
							assert(0 && "illegal instruction");
						}

						cacheEntry = oi->getInstruction() | (Nop << 8);

						if(!exec_nonParallel(oi, op))
						{
							assert( 0 && "illegal instruction" );
						}
					}
					else
					{
						const auto* oi = m_opcodes.findParallelMoveOpcodeInfo(op);
						if(!oi)
						{
							m_opcodes.findParallelMoveOpcodeInfo(op);	// retry here to help debugging
							assert(0 && "illegal instruction");
						}
						cacheEntry |= oi->getInstruction() << 8;
					}
				}
				}
			break;
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
		case MMM_AbsAddr:		return fetchOpWordB();		// absolute address
		case MMM_ImmediateData:	return fetchOpWordB();		// immediate data
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
		case 0x0f:	return getB<TReg24>();

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

		LOG( std::string(scss.str()) << " SC=" << std::hex << std::setw(6) << std::setfill('0') << (int)reg.sc.var << " pcOld=" << pcCurrentInstruction << " pcNew=" << reg.pc.var << " ictr=" << reg.ictr.var << " func=" << _func );
	}

	// _____________________________________________________________________________
	// resetSW
	//
	void DSP::resetSW()
	{
		/*
		Reset the interrupt priority register and all on-chip peripherals. This is a
		software reset, which is not equivalent to a hardware RESET since only on-chip peripherals
		and the interrupt structure are affected. The processor state is not affected, and execution
		continues with the next instruction. All interrupt sources are disabled except for the stack
		error, NMI, illegal instruction, Trap, Debug request, and hardware reset interrupts.
		*/

		perif[0]->reset();
		if(perif[1] != perif[0])
			perif[1]->reset();
	}

	void DSP::jsr(const TReg24& _val)
	{
		pushPCSR();
		setPC(_val);
	}

	// _____________________________________________________________________________
	// exec_move_ddddd_MMMRRR
	//
	void DSP::exec_move_ddddd_MMMRRR( TWord ddddd, TWord mmmrrr, bool write, EMemArea memArea )
	{
		if( write && mmmrrr == MMM_ImmediateData )
		{
			decode_ddddd_write<TReg24>( ddddd, TReg24(fetchOpWordB()) );
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
	bool DSP::do_start( TReg24 _loopcount, TWord _addr )
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

		reg.la.var = _addr;
		reg.lc = _loopcount;

		pushPCSR();

		sr_set( SR_LF );

		return true;
	}

	// _____________________________________________________________________________
	// exec_do_end
	//
	bool DSP::do_end()
	{
		// restore PC to point to the next instruction after the last instruction of the loop
		setPC(reg.la.var+1);

		// restore previous loop flag
		sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

		// decrement SP twice, restoring old loop settings
		decSP();

		reg.lc = ssl();
		reg.la = ssh();

	//	LOG( "DO END: loop flag = " << sr_test(SR_LF) << " sc=" << (int)sc.var << " lc:" << std::hex << lc.var << " la:" << std::hex << la.var );

		return true;
	}

	bool DSP::do_iterate(const uint32_t _depth)
	{
		if(!sr_test(SR_LF))
			return false;

		// DO
		if(reg.pc != reg.la )
			return false;

		if( reg.lc.var > 1 )
		{
			--reg.lc.var;
			reg.pc = hiword(reg.ss[reg.sc.toWord()]);
		}
		else
		{
			do_end();

			// nested loop update
			if(sr_test(SR_LF))
				do_iterate(_depth + 1);
		}
		return true;
	}

	bool DSP::rep_exec(const TWord loopCount)
	{
		const auto lcBackup = reg.lc;

		reg.lc.var = loopCount;

		pcCurrentInstruction = reg.pc.var;
		const auto op = fetchPC();

		while( reg.lc.var > 1 )
		{
			--reg.lc.var;
			execOp(op);
		}

		reg.lc = lcBackup;

		return true;
	}

	void DSP::decSP()
	{
		LOGSC("return");

		assert(reg.sc.var > 0);
		--reg.sp.var;
		--reg.sc.var;
	}

	void DSP::incSP()
	{
		assert(reg.sc.var < reg.ss.eSize-1);
		++reg.sp.var;
		++reg.sc.var;

		const std::string sym = mem.getSymbol(MemArea_P, reg.pc.var);
			
		LOGSC((std::string(m_asm) + " - " + sym).c_str());
	//	assert( reg.sc.var <= 9 );
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
		sr_n_update(d);
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
		sr_n_update(d);
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
		sr_n_update(d);
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
		const uint64_t v = dSrc.var & overflowMaskU;
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
		TReg56&			d = ab ? reg.b : reg.a;
		const TReg56&	s = ab ? reg.a : reg.b;

		const TReg56 old = d;
		const TInt64 res = (d.signextend<TInt64>() >> 1) + s.signextend<TInt64>();
		d.var = res;
		d.doMasking();

		sr_s_update();
		sr_e_update(d);
		sr_u_update(d);
		sr_n_update(d);
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
		sr_n_update(d);
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
		sr_n_update( d );
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
	const char* DSP::getASM(const TWord _wordA, const TWord _wordB)
	{
	#ifdef _DEBUG
		unsigned long ops[3];
		ops[0] = _wordA;
		ops[1] = _wordB;
		ops[2] = 0;
		disassemble( m_asm, ops, reg.sr.var, reg.omr.var );
	#endif
		return m_asm;
	}

	// _____________________________________________________________________________
	// memWrite
	//
	bool DSP::memWrite( EMemArea _area, TWord _offset, TWord _value )
	{
		if(_area == MemArea_P)
			m_opcodeCache[_offset] = ResolveCache;

		return mem.set( _area, _offset, _value );
	}

	bool DSP::memWritePeriph( EMemArea _area, TWord _offset, TWord _value )
	{
		perif[_area]->write(_offset, _value );
		return true;
	}
	bool DSP::memWritePeriphFFFF80( EMemArea _area, TWord _offset, TWord _value )
	{
		return memWritePeriph( _area, _offset + 0xffff80, _value );
	}
	bool DSP::memWritePeriphFFFFC0( EMemArea _area, TWord _offset, TWord _value )
	{
		return memWritePeriph( _area, _offset + 0xffffc0, _value );
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

	TWord DSP::memReadPeriph(EMemArea _area, TWord _offset) const
	{
		return perif[_area]->read(_offset);
	}
	TWord DSP::memReadPeriphFFFF80(EMemArea _area, TWord _offset) const
	{
		return memReadPeriph(_area, _offset + 0xffff80);
	}
	TWord DSP::memReadPeriphFFFFC0(EMemArea _area, TWord _offset) const
	{
		return memReadPeriph(_area, _offset + 0xffffc0);
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
		sr_n_update(d);
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
		sr_n_update(d);
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
		fwrite( &pcCurrentInstruction, 1, 1, _file );
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
		fread( &pcCurrentInstruction, 1, 1, _file );
		fread( m_asm, sizeof(m_asm), 1, _file );
		fread( &cache, sizeof(cache), 1, _file );

		return true;
	}

	void DSP::injectInterrupt(InterruptVectorAddress _interruptVectorAddress)
	{
		m_pendingInterrupts.push_back(_interruptVectorAddress);
	}
}
