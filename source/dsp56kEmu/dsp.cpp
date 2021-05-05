// DSP 56300 family 24-bit DSP emulator

#include "dsp.h"

#include <iomanip>
#include <cstring>

#include "registers.h"
#include "types.h"
#include "memory.h"
#include "disasm.h"
#include "aar.h"
#include "dspconfig.h"

#include "dsp_decode.inl"

#include "dsp_ops.inl"
#include "dsp_ops_alu.inl"
#include "dsp_ops_bra.inl"
#include "dsp_ops_jmp.inl"
#include "dsp_ops_move.inl"

#include "dsp_jumptable.inl"

#if 0
#	define LOGSC(F)	logSC(F)
#else
#	define LOGSC(F)	{}
#endif

//

namespace dsp56k
{
	constexpr bool g_traceSupported = false;

	Jumptable g_jumptable;

	// _____________________________________________________________________________
	// DSP
	//
	DSP::DSP(Memory& _memory, IPeripherals* _pX, IPeripherals* _pY)
		: mem(_memory)
		, perif({_pX, _pY})
		, pcCurrentInstruction(0xffffff)
		, m_disasm(m_opcodes)
	{
		mem.setDSP(this);

		m_disasm.addSymbols(mem);
		
		perif[0]->setDSP(this);
		perif[1]->setDSP(this);

		perif[0]->setSymbols(m_disasm);
		perif[1]->setSymbols(m_disasm);

		clearOpcodeCache();

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

		for (int i=0;i<8;i++) set_m(i, 0xFFFFFF);
		reg.r[0] = reg.r[1] = reg.r[2] = reg.r[3] = reg.r[4] = reg.r[5] = reg.r[6] = reg.r[7] = TReg24(int(0));
		reg.n[0] = reg.n[1] = reg.n[2] = reg.n[3] = reg.n[4] = reg.n[5] = reg.n[6] = reg.n[7] = TReg24(int(0));

		iprc(0);
		iprp(0);

		// TODO: The Bus Control Register (BCR), the Address Attribute Registers (AAR3–AAR0) and the DRAM Control Register (DCR) are set to their initial values as described in Chapter 9, External Memory Interface (Port A). The initial value causes a maximum number of wait states to be added to every external memory access.

		reg.sp = TReg24(int(0));
		reg.sc = TReg5(char(0));

		reg.sz.var = 0xbadbad; // The SZ register is not initialized during hardware reset, and must be set, using a MOVEC instruction, prior to enabling the stack extension.

		const CCRMask srClear	= static_cast<CCRMask>(SR_RM | SR_SM | SR_CE | SR_SA | SR_FV | SR_LF | SR_DM | SR_SC | SR_S0 | SR_S1 | 0xf);
		const CCRMask srSet		= static_cast<CCRMask>(SR_CP0 | SR_CP1 | SR_I0 | SR_I1);

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

		m_instructions = 0;
	}

	// _____________________________________________________________________________
	// exec
	//
	void DSP::exec()
	{
		// we do not support 16-bit compatibility mode
		assert( (reg.sr.var & SR_SC) == 0 && "16 bit compatibility mode is not supported");

		(this->*m_interruptFunc)();

		pcCurrentInstruction = reg.pc.toWord();

		const auto op = fetchPC();

		execOp(op);
	}

	void DSP::execPeriph()
	{
		if ((peripheralCounter--)) return;
		peripheralCounter = 20;
		perif[0]->exec();

		if(perif[1] != perif[0])
			perif[1]->exec();
	}

	void DSP::execInterrupts()
	{
		if(!m_pendingInterrupts.empty())
		{
			// TODO: priority sorting, masking
			const auto minPrio = mr().var & 0x3;

			if(minPrio < 3)
			{
				const auto vba = m_pendingInterrupts.pop_front();
				TWord op0, op1;
				memReadOpcode(vba, op0, op1);

				const auto oldSP = reg.sp.var;

				m_opWordB = op1;

				m_processingMode = FastInterrupt;

				pcCurrentInstruction = vba;
				execOp(op0);

				const auto jumped = reg.sp.var - oldSP;

				// only exec the second op if the first one was a one-word op and we did not jump into a long interrupt
				if(m_currentOpLen == 1 && !jumped)
				{
					pcCurrentInstruction = vba+1;
					m_opWordB = 0;
					execOp(op1);

					// fast interrupt done
					m_processingMode = DefaultPreventInterrupt;
					m_interruptFunc = &DSP::execDefaultPreventInterrupt;
				}
				else if(jumped)
				{
					// Long Interrupt

					sr_clear((CCRMask)(SR_S1|SR_S0|SR_SA));

					m_processingMode = LongInterrupt;
					m_interruptFunc = &DSP::nop;
				}
				else
				{
					// Default Processing, no interrupt
					m_processingMode = DefaultPreventInterrupt;
					m_interruptFunc = &DSP::execDefaultPreventInterrupt;
				}
			}
		}
	}

	void DSP::execDefaultPreventInterrupt()
	{
		m_processingMode = Default;

		if(m_pendingInterrupts.empty())
			m_interruptFunc = &DSP::execNoPendingInterrupts;
		else
			m_interruptFunc = &DSP::execInterrupts;
	}

	void DSP::execNoPendingInterrupts()
	{
		execPeriph();
	}

	std::string DSP::getSSindent() const
	{
		std::stringstream ss;

		for(TWord i=0; i<ssIndex(); ++i)
			ss << "\t";

		return std::string(ss.str());
	}

	void DSP::execOp(const TWord op)
	{
		getASM(op, m_opWordB);

		m_currentOpLen = 1;

		const TWord currentOp = pcCurrentInstruction;
		const auto& opCache = m_opcodeCache[currentOp];

		exec_jump(opCache.op, op);

		++m_instructions;

		if(g_traceSupported && pcCurrentInstruction == currentOp)
			traceOp();
	}

	void DSP::exec_jump(const TInstructionFunc& _func, TWord op)
	{
		(this->*_func)(op);
	}

	bool DSP::exec_parallel(const TInstructionFunc& funcMove, const TInstructionFunc& funcAlu, const TWord op)
	{
		// simulate latches registers for parallel instructions
		const auto preMoveX = reg.x;
		const auto preMoveY = reg.y;
		const auto preMoveA = reg.a;
		const auto preMoveB = reg.b;

		exec_jump(funcMove, op);

		const auto postMoveX = reg.x;
		const auto postMoveY = reg.y;
		const auto postMoveA = reg.a;
		const auto postMoveB = reg.b;

		// restore previous state for the ALU to process them
		reg.x = preMoveX;
		reg.y = preMoveY;
		reg.a = preMoveA;
		reg.b = preMoveB;

		exec_jump(funcAlu, op);

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
		return true;
	}

	// _____________________________________________________________________________
	// logSC
	//
	void DSP::logSC( const char* _func ) const
	{
		if( strstr(_func, "DO" ) )
			return;

		const std::string indent = getSSindent();

		LOG( indent << " SC=" << std::hex << std::setw(6) << std::setfill('0') << (int)reg.sc.var << " pcOld=" << pcCurrentInstruction << " pcNew=" << reg.pc.var << " ictr=" << (m_instructions & 0xffffff) << " func=" << _func );
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

	void DSP::setCCRDirty(bool ab, const TReg56& _alu, uint32_t _dirtyBitsMask)
	{
//		if(ccrCache.dirty && ccrCache.ab != ab)
//			updateDirtyCCR();

		ccrCache.dirty |= _dirtyBitsMask;
		ccrCache.alu = _alu;
		ccrCache.ab = ab;
	}

	void DSP::updateDirtyCCR() const
	{
		if(!ccrCache.dirty)
			return;

		auto& dsp = const_cast<DSP&>(*this);

		dsp.ccrCache.dirty = 0;
		
		dsp.sr_s_update();
		dsp.sr_e_update(ccrCache.alu);
		dsp.sr_u_update(ccrCache.alu);
		dsp.sr_n_update(ccrCache.alu);
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
	bool DSP::do_exec( TWord _loopcount, TWord _addr )
	{
	//	LOG( "DO BEGIN: " << (int)sc.var << ", loop flag = " << sr_test(SR_LF) );

		if( !_loopcount )
		{
			if( sr_test_noCache( SR_SC ) )
				_loopcount = 65536;
			else
			{
				setPC(_addr+1);
				return true;
			}
		}

		ssh(reg.la);
		ssl(reg.lc);

		reg.la.var = _addr;
		reg.lc.var = _loopcount;

		pushPCSR();

		const auto stackCount = reg.sc.var;
		
		sr_set( SR_LF );

		traceOp();
		TWord looplen=_addr + 1  - getPC().var;
		uint32_t simpleguess=getInstructionCounter() + looplen;
		if (looplen>16) simpleguess=0;

		// __________________
		//

		while(sr_test_noCache(SR_LF) && reg.sc.var >= stackCount)
		{
			exec();

			if(reg.pc.var != (reg.la.var+1))
				continue;

			if( reg.lc.var <= 1 )
			{
				// restore PC to point to the next instruction after the last instruction of the loop
				setPC(reg.la.var+1);

				do_end();
				break;
			}

			if (getInstructionCounter() == simpleguess)
			{
				const TWord startpc=hiword(reg.ss[ssIndex()]).var;
				while (--reg.lc.var>0) {
					setPC(startpc);
					for (int i=0;i<looplen;i++)
					{
						pcCurrentInstruction = reg.pc.toWord();
						execOp(fetchPC());
					}
				}
				setPC(reg.la.var+1);
				do_end();
				return true;
			}
			--reg.lc.var;
			setPC(hiword(reg.ss[ssIndex()]));
		}
		return true;
	}

	// _____________________________________________________________________________
	// exec_do_end
	//
	bool DSP::do_end()
	{
		// restore previous loop flag
		sr_toggle( SR_LF, (ssl().var & SR_LF) != 0 );

		// decrement SP twice, restoring old loop settings
		decSP();

		reg.lc = ssl();
		reg.la = ssh();

	//	LOG( "DO END: loop flag = " << sr_test(SR_LF) << " sc=" << (int)sc.var << " lc:" << std::hex << lc.var << " la:" << std::hex << la.var );

		return true;
	}

	bool DSP::rep_exec(const TWord loopCount)
	{
		const auto lcBackup = reg.lc;
		reg.lc.var = loopCount;

		traceOp();

		pcCurrentInstruction = reg.pc.var;
		const auto op = fetchPC();

		--reg.lc.var;
		execOp(op);

		const auto& opCache = m_opcodeCache[pcCurrentInstruction];

		const auto& func = opCache.op;

		while( reg.lc.var > 0 )
		{
			--reg.lc.var;
			(this->*func)(op);
			++m_instructions;
			traceOp();
		}

		reg.lc = lcBackup;

		return true;
	}

	void DSP::traceOp()
	{
		if(!g_traceSupported || !m_trace)
			return;

		const auto op = memRead(MemArea_P, pcCurrentInstruction);

		std::stringstream ss;
		ss << "p:$" << HEX(pcCurrentInstruction) << ' ' << HEX(op);
		if(m_currentOpLen > 1)
			ss << ' ' << HEX(m_opWordB);
		else
			ss << "       ";
		ss << " = ";
		if(m_trace & StackIndent)
			ss << getSSindent();
		ss << m_asm;
		const std::string str(ss.str());
		LOGF(str);

		if(m_trace & Regs)
		{
			dumpRegisters();
			updatePreviousRegisterStates();
		}
	}

	void DSP::decSP()
	{
		LOGSC("return");

		assert(ssIndex() > 0);
		--reg.sp.var;
		--reg.sc.var;
	}

	void DSP::incSP()
	{
		assert(ssIndex() < reg.ss.eSize-1);
		++reg.sp.var;
		++reg.sc.var;

	//	assert( reg.sc.var <= 9 );
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
		case Reg_SR:	_res = getSR();		break;
		case Reg_OMR:	_res = reg.omr;		break;
		case Reg_SP:	_res = reg.sp;		break;

		case Reg_LA:	_res = reg.la;		break;
		case Reg_LC:	_res = reg.lc;		break;

		case Reg_ICTR:	_res.var = m_instructions & 0xffffff;	break;

		case Reg_SSH:	_res = hiword(reg.ss[ssIndex()]);	break;
		case Reg_SSL:	_res = loword(reg.ss[ssIndex()]);	break;

		case Reg_CNT1:	_res.var = 0; /*reg.cnt1;*/	break;
		case Reg_CNT2:	_res.var = 0; /*reg.cnt2;*/	break;
		case Reg_CNT3:	_res.var = 0; /*reg.cnt3;*/	break;
		case Reg_CNT4:	_res.var = 0; /*reg.cnt4;*/	break;

		case Reg_VBA:	_res = reg.vba;			break;
		case Reg_SZ:	_res = reg.sz;			break;
		case Reg_EP:	_res = reg.ep;			break;
		case Reg_DCR:	_res.var = 0;/*reg.dcr;*/	break;
		case Reg_BCR:	_res.var = 0;/*reg.bcr;*/	break;
		case Reg_IPRP:	_res.var = iprp();		break;
		case Reg_IPRC:	_res.var = iprc();		break;

		case Reg_AAR0:	_res.var = memReadPeriph(MemArea_X, M_AAR0);		break;
		case Reg_AAR1:	_res.var = memReadPeriph(MemArea_X, M_AAR1);		break;
		case Reg_AAR2:	_res.var = memReadPeriph(MemArea_X, M_AAR2);		break;
		case Reg_AAR3:	_res.var = memReadPeriph(MemArea_X, M_AAR3);		break;

		case Reg_REPLACE:	_res.var = 0;/* reg.replace; */	break;
		case Reg_HIT:		_res.var = 0;/* reg.hit;	 */	break;
		case Reg_MISS:		_res.var = 0;/* reg.miss;	 */	break;
		case Reg_CYC:		_res.var = 0;/* reg.cyc;	 */	break;
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
	bool DSP::readRegToInt( EReg _reg, int64_t& _dst ) const
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
							
		case Reg_M0:	set_m(0, _res.var);		break;
		case Reg_M1:	set_m(1, _res.var);		break;
		case Reg_M2:	set_m(2, _res.var);		break;
		case Reg_M3:	set_m(3, _res.var);		break;
		case Reg_M4:	set_m(4, _res.var);		break;
		case Reg_M5:	set_m(5, _res.var);		break;
		case Reg_M6:	set_m(6, _res.var);		break;
		case Reg_M7:	set_m(7, _res.var);		break;
							
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
//	#ifdef _DEBUG
		if (m_trace && g_traceSupported) m_disasm.disassemble(m_asm, _wordA, _wordB, 0, 0, pcCurrentInstruction);
//	#endif
		return m_asm.c_str();
	}

	// _____________________________________________________________________________
	// memWrite
	//
	bool DSP::memWrite( EMemArea _area, TWord _offset, TWord _value )
	{
		aarTranslate(_area, _offset);
	
		const auto res = mem.dspWrite( _area, _offset, _value );

		if(_area == MemArea_P && _offset < m_opcodeCache.size())
			m_opcodeCache[_offset].op = &DSP::op_ResolveCache;

		return res;
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

		aarTranslate(_area, _offset);

		return mem.get(_area, _offset);
	}

	void DSP::memReadOpcode(TWord _offset, TWord& _wordA, TWord& _wordB) const
	{
		aarTranslate(MemArea_P, _offset);
		mem.getOpcode(_offset, _wordA, _wordB);
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

	void DSP::aarTranslate(EMemArea _area, TWord& _offset) const
	{
		if(!g_useAARTranslate)
			return;

		// TODO: probably not as generic as it should be
		if(_offset < 0x3800)
			return;

		constexpr AARRegisters aarRegs[4] = {M_AAR0, M_AAR1, M_AAR2, M_AAR3};
		constexpr uint32_t areaEnabled[3] = {M_BXEN, M_BYEN, M_BPEN};

		auto o = _offset;

		for(int i=3; i>=0; --i)
		{
			const auto aar = memReadPeriph(MemArea_X, aarRegs[i]);

			if(!bittest(aar, areaEnabled[_area]))
				continue;

			const auto compareValue = aar & M_BAC;
			const auto compareBitCount = (aar & M_BNC) >> 8;

			const auto mask = static_cast<int32_t>(0xff000000) >> compareBitCount;

			const auto match = (_offset & mask) == (compareValue & mask);

			if(match)
			{
				o = (_offset & 0xffff) | ((i+2)<<16);
				break;
			}
		}

		if(o != _offset)
			LOG("AAR translated: " << HEX(_offset) << " => " << HEX(o));

		_offset = o;
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

		sr_z_update(d);
	//	sr_v_update(d);
	//	sr_l_update_by_v();
		setCCRDirty(ab, d, SR_S | SR_E | SR_U | SR_N);
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

		auto d64 = d.signextend<TInt64>();
		d64 = -d64;
		
		d.var = d64 & 0x00ffffffffffffff;

		sr_z_update(d);
	//	TODO: how to update v? test in sim		sr_v_update(d);
		sr_l_update_by_v();
		setCCRDirty(ab, d, SR_S | SR_E | SR_U | SR_N);
	}

	void DSP::alu_not(const bool ab)
	{
		auto& d = ab ? reg.b.var : reg.a.var;

		const auto masked = ~d & 0x00ffffff000000;

		d &= 0xff000000ffffff;
		d |= masked;

		sr_toggle(SRB_N, bitvalue<uint64_t, 47>(d));	// Set if bit 47 of the result is set
		sr_toggle(SR_Z, masked == 0);					// Set if bits 47–24 of the result are 0
		sr_clear(SR_V);									// Always cleared
		sr_s_update();									// Changed according to the standard definition
		//sr_l_update_by_v();							// Changed according to the standard definition
	}

	void DSP::set_m( const int which, const TWord val)
	{
		reg.m[which].var = val;
		if (val == 0xffffff) {
			modulo[which]=0;
			moduloMask[which]=0xffffff;
			return;
		}
		const TWord moduloTest = (val & 0xffff);
		if (moduloTest == 0)
		{
			moduloMask[which] = 0;
		}
		else if( moduloTest <= 0x007fff )
		{
			moduloMask[which] = AGU::calcModuloMask(val);
			modulo[which] = val + 1;
		}
		else
		{
			LOG_ERR_NOTIMPLEMENTED( "AGU multiple Wrap-Around Modulo Modifier: m=" << HEX(val) );
		}
	}


	// _____________________________________________________________________________
	// save
	//
	bool DSP::save( FILE* _file ) const
	{
		fwrite( &reg, sizeof(reg), 1, _file );
		fwrite( &pcCurrentInstruction, 1, 1, _file );
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
		fread( &cache, sizeof(cache), 1, _file );

		return true;
	}

	void DSP::injectInterrupt(uint32_t _interruptVectorAddress)
	{
//		assert(!m_pendingInterrupts.full());
		m_pendingInterrupts.push_back(_interruptVectorAddress);

		if(m_interruptFunc == &DSP::execNoPendingInterrupts)
			m_interruptFunc = &DSP::execInterrupts;
	}

	void DSP::clearOpcodeCache()
	{
		m_opcodeCache.clear();
		m_opcodeCache.resize(mem.size(), {&DSP::op_ResolveCache});
	}

	void DSP::clearOpcodeCache(TWord _address)
	{
		m_opcodeCache[_address].op = &DSP::op_ResolveCache;
	}
	
	TInstructionFunc DSP::resolvePermutation(const Instruction _inst, const TWord _op) const
	{
		const auto funcIndex = g_jumptable.resolve(_inst, _op);
		return g_jumptable.jumptable()[funcIndex];
	}

	void DSP::dumpRegisters() const
	{
		std::stringstream ss;
		dumpRegisters(ss);
		const auto str(ss.str());
		LOGF(str);
	}
	void DSP::dumpRegisters(std::stringstream& _ss) const
	{
		auto logReg = [this](EReg _reg, int _width)
		{
			std::stringstream ss;
			int64_t v;
			if(!readRegToInt(_reg, v))
			{
				readRegToInt(_reg, v);
				assert(false);
			}
			const bool c = m_prevRegStates[_reg].val != v;
			if(c)
				ss << '{';
			if(_width > 0)
				ss << '$' << std::hex << std::setfill('0') << std::setw(_width) << v;
			else
				ss << std::setfill('0') << std::setw(-_width) << v;
			if(c)
				ss << '}';
			const std::string res(ss.str());
			return res;
		};

		_ss << "   x=       " << logReg(Reg_X, 12) << "    y=       " << logReg(Reg_Y, 12) << std::endl;
		_ss << "   a=     " << logReg(Reg_A, 14) << "    b=     " << logReg(Reg_B, 14) << std::endl;
		_ss << "               x1=" << logReg(Reg_X1, 6) << "   x0=" << logReg(Reg_X0, 6) << "   r7=" << logReg(Reg_R7,6) << " n7=" << logReg(Reg_N7,6) << " m7=" << logReg(Reg_M7,6) << std::endl;
		_ss << "               y1=" << logReg(Reg_Y1, 6) << "   y0=" << logReg(Reg_Y0, 6) << "   r6=" << logReg(Reg_R6,6) << " n6=" << logReg(Reg_N6,6) << " m6=" << logReg(Reg_M6,6) << std::endl;
		_ss << "  a2=    " << logReg(Reg_A2, 2) << "   a1=" << logReg(Reg_A1, 6) << "   a0=" << logReg(Reg_A0,6) << "   r5=" << logReg(Reg_R5,6) << " n5=" << logReg(Reg_N5,6) << " m5=" << logReg(Reg_M5,6) << std::endl;
		_ss << "  b2=    " << logReg(Reg_B2, 2) << "   b1=" << logReg(Reg_B1, 6) << "   b0=" << logReg(Reg_B0,6) << "   r4=" << logReg(Reg_R4,6) << " n4=" << logReg(Reg_N4,6) << " m4=" << logReg(Reg_M4,6) << std::endl;
		_ss << "                                         r3=" << logReg(Reg_R3,6) << " n3=" << logReg(Reg_N3,6) << " m3=" << logReg(Reg_M3,6) << std::endl;
		_ss << "  pc=" << logReg(Reg_PC, 6) << "   sr=" << logReg(Reg_SR, 6) << "  omr=" << logReg(Reg_OMR,6) << "   r2=" << logReg(Reg_R2,6) << " n2=" << logReg(Reg_N2,6) << " m2=" << logReg(Reg_M2,6) << std::endl;
		_ss << "  la=" << logReg(Reg_LA, 6) << "   lc=" << logReg(Reg_LC, 6) << "                r1=" << logReg(Reg_R1,6) << " n1=" << logReg(Reg_N1,6) << " m1=" << logReg(Reg_M1,6) << std::endl;
		_ss << " ssh=" << logReg(Reg_SSH, 6) << "  ssl=" << logReg(Reg_SSL, 6) << "   sp=" << logReg(Reg_SP,6) << "   r0=" << logReg(Reg_R0,6) << " n0=" << logReg(Reg_N0,6) << " m0=" << logReg(Reg_M0,6) << std::endl;
		_ss << "  ep=" << logReg(Reg_EP, 6) << "   sz=" << logReg(Reg_SZ, 6) << "   sc=" << logReg(Reg_SC,6) << "  vba=" << logReg(Reg_VBA,6) << std::endl;
		_ss << "iprc=" << logReg(Reg_IPRC, 6) << " iprp=" << logReg(Reg_IPRP, 6) << "  bcr=" << logReg(Reg_BCR,6) << "  dcr=" << logReg(Reg_DCR,6) << std::endl;
		_ss << "aar0=" << logReg(Reg_AAR0, 6) << " aar1=" << logReg(Reg_AAR1, 6) << " aar2=" << logReg(Reg_AAR2,6) << " aar3=" << logReg(Reg_AAR3,6) << std::endl;
		_ss << "  hit=   " << logReg(Reg_HIT, -6) << "   miss=   " << logReg(Reg_MISS, -6) << " replace=" << logReg(Reg_REPLACE,-6) << std::endl;
		_ss << "  cyc=   " << logReg(Reg_CYC, -6) << "   ictr=   " << logReg(Reg_ICTR, -6) << std::endl;
		_ss << " cnt1=   " << logReg(Reg_CNT1, -6) << "   cnt2=   " << logReg(Reg_CNT2, -6) << " cnt3=   " << logReg(Reg_CNT3,-6) << "  cnt4=   " << logReg(Reg_CNT4,-6) << std::endl;
/*
   x=       $000000000000    y=       $000000000000
   a=     $00000000000051    b=     $00000000000000
               x1=$000000   x0=$000000   r7=$000000 n7=$000000 m7=$ffffff
               y1=$000000   y0=$000000   r6=$000000 n6=$000000 m6=$ffffff
  a2=    $00   a1=$000000   a0=$000051   r5=$000000 n5=$000000 m5=$ffffff
  b2=    $00   b1=$000000   b0=$000000   r4=$000000 n4=$000000 m4=$ffffff
                                         r3=$000000 n3=$000000 m3=$ffffff
  pc=$000102   sr=$c00300  omr=$00030e   r2=$000000 n2=$000000 m2=$ffffff
  la=$ffffff   lc=$000000                r1=$000100 n1=$000000 m1=$ffffff
 ssh=$000000  ssl=$000000   sp=$000000   r0=$000151 n0=$000000 m0=$ffffff
  ep=$000000   sz=$000000   sc=$000000  vba=$000000
iprc=$000000 iprp=$000000  bcr=$212421  dcr=$000000
aar0=$000008 aar1=$000000 aar2=$000000 aar3=$000000
  hit=   000000   miss=   000000 replace=000000
  cyc=   001027   ictr=   000259
 cnt1=   000000   cnt2=   000000 cnt3=   000000  cnt4=   000000
*/
	}

	void DSP::errNotImplemented(const char* _opName)
	{
		std::stringstream ss; ss << std::endl;
		ss << "Not Implemented: " << _opName << std::endl;
		coreDump(ss);

		const auto str(ss.str());
		LOG(str);

		assert(false && "instruction not implemented, see console for details");
	}

	void DSP::updatePreviousRegisterStates()
	{
		for( size_t i=0; i<Reg_COUNT; ++i )
		{
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
				regChange.ictr = m_instructions & 0xffffff;

				m_regChanges.push_back( regChange );

				m_prevRegStates[i].val = regVal;
			}
		}
	}

	void DSP::coreDump(std::stringstream& _ss)
	{
		TWord opA, opB;
		memReadOpcode(pcCurrentInstruction, opA, opB);

		std::string op;
		m_disasm.disassemble(op, opA, opB, 0, 0, pcCurrentInstruction);

		_ss << "Current Instruction: " << op << std::endl;
		_ss << "Current Opcode: $" << HEX(opA) << " ($" << opB << ")" << std::endl;
		_ss << std::endl;
		_ss << "Stack:" << std::endl;
		int s = ssIndex();

		while(s >= 0)
		{
			const auto pc = hiword(reg.ss[s]).var;
			const auto sr = loword(reg.ss[s]).var;
			_ss << '$' << std::setw(2) << std::setfill('0') << std::hex << s << ": ";
			_ss << '$' << HEX(pc) << ":$" << HEX(sr) << " - pc:$" << HEX(pc) << " - ";

			memReadOpcode(pc, opA, opB);
			m_disasm.disassemble(op, opA, opB, 0, 0, pc);
			_ss << op << std::endl;
			--s;
		}
		_ss << std::endl;
		_ss << "Registers:" << std::endl;
		updatePreviousRegisterStates();
		dumpRegisters(_ss);
	}

	void DSP::coreDump()
	{
		std::stringstream ss;
		coreDump(ss);
		const std::string dump(ss.str());
		LOG(std::endl << dump);
	}
}
