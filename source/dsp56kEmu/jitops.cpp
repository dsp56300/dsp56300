#include "jitops.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitblockruntimedata.h"
#include "jithelper.h"

#include "jitops_alu.inl"
#include "opcodecycles.h"

#include "opcodes.h"

namespace dsp56k
{
	struct OpcodeInfo;
	using TJitOpcodeFunc = void (JitOps::*)(TWord op);

	constexpr TJitOpcodeFunc g_opcodeFuncs[] =
	{
		&JitOps::op_Abs,							// Abs
		&JitOps::op_ADC,							// ADC 
		&JitOps::op_Add_SD,							// Add_SD
		&JitOps::op_Add_xx,							// Add_xx
		&JitOps::op_Add_xxxx,						// Add_xxxx 
		&JitOps::op_Addl,							// Addl 
		&JitOps::op_Addr,							// Addr 
		&JitOps::op_And_SD,							// And_SD 
		&JitOps::op_And_xx,							// And_xx 
		&JitOps::op_And_xxxx,						// And_xxxx 
		&JitOps::op_Andi,							// Andi 
		&JitOps::op_Asl_D,							// Asl_D 
		&JitOps::op_Asl_ii,							// Asl_ii 
		&JitOps::op_Asl_S1S2D,						// Asl_S1S2D 
		&JitOps::op_Asr_D,							// Asr_D 
		&JitOps::op_Asr_ii,							// Asr_ii 
		&JitOps::op_Asr_S1S2D,						// Asr_S1S2D 
		&JitOps::op_Bcc_xxxx,						// Bcc_xxxx 
		&JitOps::op_Bcc_xxx,						// Bcc_xxx 
		&JitOps::op_Bcc_Rn,							// Bcc_Rn 
		&JitOps::op_Bchg_ea,						// Bchg_ea 
		&JitOps::op_Bchg_aa,						// Bchg_aa 
		&JitOps::op_Bchg_pp,						// Bchg_pp 
		&JitOps::op_Bchg_qq,						// Bchg_qq 
		&JitOps::op_Bchg_D,							// Bchg_D 
		&JitOps::op_Bclr_ea,						// Bclr_ea 
		&JitOps::op_Bclr_aa,						// Bclr_aa 
		&JitOps::op_Bclr_pp,						// Bclr_pp 
		&JitOps::op_Bclr_qq,						// Bclr_qq 
		&JitOps::op_Bclr_D,							// Bclr_D 
		&JitOps::op_Bra_xxxx,						// Bra_xxxx 
		&JitOps::op_Bra_xxx,						// Bra_xxx 
		&JitOps::op_Bra_Rn,							// Bra_Rn 
		&JitOps::op_Brclr_ea,						// Brclr_ea 
		&JitOps::op_Brclr_aa,						// Brclr_aa 
		&JitOps::op_Brclr_pp,						// Brclr_pp 
		&JitOps::op_Brclr_qq,						// Brclr_qq 
		&JitOps::op_Brclr_S,						// Brclr_S 
		&JitOps::op_BRKcc,							// BRKcc 
		&JitOps::op_Brset_ea,						// Brset_ea 
		&JitOps::op_Brset_aa,						// Brset_aa 
		&JitOps::op_Brset_pp,						// Brset_pp 
		&JitOps::op_Brset_qq,						// Brset_qq 
		&JitOps::op_Brset_S,						// Brset_S 
		&JitOps::op_BScc_xxxx,						// BScc_xxxx 
		&JitOps::op_BScc_xxx,						// BScc_xxx 
		&JitOps::op_BScc_Rn,						// BScc_Rn 
		&JitOps::op_Bsclr_ea,						// Bsclr_ea 
		&JitOps::op_Bsclr_aa,						// Bsclr_aa 
		&JitOps::op_Bsclr_pp,						// Bsclr_pp 
		&JitOps::op_Bsclr_qq,						// Bsclr_qq 
		&JitOps::op_Bsclr_S,						// Bsclr_S 
		&JitOps::op_Bset_ea,						// Bset_ea 
		&JitOps::op_Bset_aa,						// Bset_aa 
		&JitOps::op_Bset_pp,						// Bset_pp 
		&JitOps::op_Bset_qq,						// Bset_qq 
		&JitOps::op_Bset_D,							// Bset_D 
		&JitOps::op_Bsr_xxxx,						// Bsr_xxxx 
		&JitOps::op_Bsr_xxx,						// Bsr_xxx 
		&JitOps::op_Bsr_Rn,							// Bsr_Rn 
		&JitOps::op_Bsset_ea,						// Bsset_ea 
		&JitOps::op_Bsset_aa,						// Bsset_aa 
		&JitOps::op_Bsset_pp,						// Bsset_pp 
		&JitOps::op_Bsset_qq,						// Bsset_qq 
		&JitOps::op_Bsset_S,						// Bsset_S 
		&JitOps::op_Btst_ea,						// Btst_ea 
		&JitOps::op_Btst_aa,						// Btst_aa 
		&JitOps::op_Btst_pp,						// Btst_pp 
		&JitOps::op_Btst_qq,						// Btst_qq 
		&JitOps::op_Btst_D,							// Btst_D 
		&JitOps::op_Clb,							// Clb 
		&JitOps::op_Clr,							// Clr 
		&JitOps::op_Cmp_S1S2,						// Cmp_S1S2 
		&JitOps::op_Cmp_xxS2,						// Cmp_xxS2 
		&JitOps::op_Cmp_xxxxS2,						// Cmp_xxxxS2 
		&JitOps::op_Cmpm_S1S2,						// Cmpm_S1S2 
		&JitOps::op_Cmpu_S1S2,						// Cmpu_S1S2 
		&JitOps::op_Debug,							// Debug 
		&JitOps::op_Debugcc,						// Debugcc 
		&JitOps::op_Dec,							// Dec 
		&JitOps::op_Div,							// Div 
		&JitOps::op_Dmac,							// Dmac 
		&JitOps::op_Do_ea,							// Do_ea 
		&JitOps::op_Do_aa,							// Do_aa 
		&JitOps::op_Do_xxx,							// Do_xxx 
		&JitOps::op_Do_S,							// Do_S 
		&JitOps::op_DoForever,						// DoForever 
		&JitOps::op_Dor_ea,							// Dor_ea 
		&JitOps::op_Dor_aa,							// Dor_aa 
		&JitOps::op_Dor_xxx,						// Dor_xxx 
		&JitOps::op_Dor_S,							// Dor_S 
		&JitOps::op_DorForever,						// DorForever 
		&JitOps::op_Enddo,							// Enddo 
		&JitOps::op_Eor_SD,							// Eor_SD 
		&JitOps::op_Eor_xx,							// Eor_xx 
		&JitOps::op_Eor_xxxx,						// Eor_xxxx 
		&JitOps::op_Extract_S1S2,					// Extract_S1S2 
		&JitOps::op_Extract_CoS2,					// Extract_CoS2 
		&JitOps::op_Extractu_S1S2,					// Extractu_S1S2 
		&JitOps::op_Extractu_CoS2,					// Extractu_CoS2 
		&JitOps::op_Ifcc<true>,						// Ifcc 
		&JitOps::op_Ifcc<false>,					// Ifcc_U 
		&JitOps::op_Illegal,						// Illegal 
		&JitOps::op_Inc,							// Inc 
		&JitOps::op_Insert_S1S2,					// Insert_S1S2 
		&JitOps::op_Insert_CoS2,					// Insert_CoS2 
		&JitOps::op_Jcc_xxx,						// Jcc_xxx 
		&JitOps::op_Jcc_ea,							// Jcc_ea 
		&JitOps::op_Jclr_ea,						// Jclr_ea 
		&JitOps::op_Jclr_aa,						// Jclr_aa 
		&JitOps::op_Jclr_pp,						// Jclr_pp 
		&JitOps::op_Jclr_qq,						// Jclr_qq 
		&JitOps::op_Jclr_S,							// Jclr_S 
		&JitOps::op_Jmp_ea,							// Jmp_ea 
		&JitOps::op_Jmp_xxx,						// Jmp_xxx 
		&JitOps::op_Jscc_xxx,						// Jscc_xxx 
		&JitOps::op_Jscc_ea,						// Jscc_ea 
		&JitOps::op_Jsclr_ea,						// Jsclr_ea 
		&JitOps::op_Jsclr_aa,						// Jsclr_aa 
		&JitOps::op_Jsclr_pp,						// Jsclr_pp 
		&JitOps::op_Jsclr_qq,						// Jsclr_qq 
		&JitOps::op_Jsclr_S,						// Jsclr_S 
		&JitOps::op_Jset_ea,						// Jset_ea 
		&JitOps::op_Jset_aa,						// Jset_aa 
		&JitOps::op_Jset_pp,						// Jset_pp 
		&JitOps::op_Jset_qq,						// Jset_qq 
		&JitOps::op_Jset_S,							// Jset_S 
		&JitOps::op_Jsr_ea,							// Jsr_ea 
		&JitOps::op_Jsr_xxx,						// Jsr_xxx 
		&JitOps::op_Jsset_ea,						// Jsset_ea 
		&JitOps::op_Jsset_aa,						// Jsset_aa 
		&JitOps::op_Jsset_pp,						// Jsset_pp 
		&JitOps::op_Jsset_qq,						// Jsset_qq 
		&JitOps::op_Jsset_S,						// Jsset_S 
		&JitOps::op_Lra_Rn,							// Lra_Rn 
		&JitOps::op_Lra_xxxx,						// Lra_xxxx 
		&JitOps::op_Lsl_D,							// Lsl_D 
		&JitOps::op_Lsl_ii,							// Lsl_ii 
		&JitOps::op_Lsl_SD,							// Lsl_SD 
		&JitOps::op_Lsr_D,							// Lsr_D 
		&JitOps::op_Lsr_ii,							// Lsr_ii 
		&JitOps::op_Lsr_SD,							// Lsr_SD 
		&JitOps::op_Lua_ea,							// Lua_ea 
		&JitOps::op_Lua_Rn,							// Lua_Rn 
		&JitOps::op_Mac_S1S2,						// Mac_S1S2 
		&JitOps::op_Mac_S<Mac_S, true, false>,		// Mac_S 
		&JitOps::op_Maci_xxxx,						// Maci_xxxx 
		&JitOps::op_Mpy_su<Macsu, true>,			// Macsu 
		&JitOps::op_Macr_S1S2,						// Macr_S1S2 
		&JitOps::op_Mac_S<Mac_S, true, true>,		// Macr_S 
		&JitOps::op_Macri_xxxx,						// Macri_xxxx 
		&JitOps::op_Max,							// Max 
		&JitOps::op_Maxm,							// Maxm 
		&JitOps::op_Merge,							// Merge 
		&JitOps::op_Move_Nop,						// Move_Nop 
		&JitOps::op_Move_xx,						// Move_xx 
		&JitOps::op_Mover,							// Mover 
		&JitOps::op_Move_ea,						// Move_ea 
		&JitOps::op_Movex_ea,						// Movex_ea 
		&JitOps::op_Movex_aa,						// Movex_aa 
		&JitOps::op_Movex_Rnxxxx,					// Movex_Rnxxxx 
		&JitOps::op_Movex_Rnxxx,					// Movex_Rnxxx 
		&JitOps::op_Movexr_ea,						// Movexr_ea 
		&JitOps::op_Movexr_A,						// Movexr_A 
		&JitOps::op_Movey_ea,						// Movey_ea 
		&JitOps::op_Movey_aa,						// Movey_aa 
		&JitOps::op_Movey_Rnxxxx,					// Movey_Rnxxxx 
		&JitOps::op_Movey_Rnxxx,					// Movey_Rnxxx 
		&JitOps::op_Moveyr_ea,						// Moveyr_ea 
		&JitOps::op_Moveyr_A,						// Moveyr_A 
		&JitOps::op_Movel_ea,						// Movel_ea 
		&JitOps::op_Movel_aa,						// Movel_aa 
		&JitOps::op_Movexy,							// Movexy 
		&JitOps::op_Movec_ea,						// Movec_ea 
		&JitOps::op_Movec_aa,						// Movec_aa 
		&JitOps::op_Movec_S1D2,						// Movec_S1D2 
		&JitOps::op_Movec_xx,						// Movec_xx 
		&JitOps::op_Movem_ea,						// Movem_ea 
		&JitOps::op_Movem_aa,						// Movem_aa 
		&JitOps::op_Movep_ppea,						// Movep_ppea 
		&JitOps::op_Movep_Xqqea,					// Movep_Xqqea 
		&JitOps::op_Movep_Yqqea,					// Movep_Yqqea 
		&JitOps::op_Movep_eapp,						// Movep_eapp 
		&JitOps::op_Movep_eaqq,						// Movep_eaqq 
		&JitOps::op_Movep_Spp,						// Movep_Spp 
		&JitOps::op_Movep_SXqq,						// Movep_SXqq 
		&JitOps::op_Movep_SYqq,						// Movep_SYqq 
		&JitOps::op_Mpy_S1S2D,						// Mpy_S1S2D 
		&JitOps::op_Mac_S<Mpy_SD, false, false>,	// Mpy_SD 
		&JitOps::op_Mpy_su<Mpy_su,false>,			// Mpy_su 
		&JitOps::op_Mpyi,							// Mpyi 
		&JitOps::op_Mpyr_S1S2D,						// Mpyr_S1S2D 
		&JitOps::op_Mac_S<Mpyr_SD, false, true>,	// Mpyr_SD 
		&JitOps::op_Mpyri,							// Mpyri 
		&JitOps::op_Neg,							// Neg 
		&JitOps::op_Nop,							// Nop 
		&JitOps::op_Norm,							// Norm 
		&JitOps::op_Normf,							// Normf 
		&JitOps::op_Not,							// Not 
		&JitOps::op_Or_SD,							// Or_SD 
		&JitOps::op_Or_xx,							// Or_xx 
		&JitOps::op_Or_xxxx,						// Or_xxxx 
		&JitOps::op_Ori,							// Ori 
		&JitOps::op_Pflush,							// Pflush 
		&JitOps::op_Pflushun,						// Pflushun 
		&JitOps::op_Pfree,							// Pfree 
		&JitOps::op_Plock,							// Plock 
		&JitOps::op_Plockr,							// Plockr 
		&JitOps::op_Punlock,						// Punlock 
		&JitOps::op_Punlockr,						// Punlockr 
		&JitOps::op_Rep_ea,							// Rep_ea 
		&JitOps::op_Rep_aa,							// Rep_aa 
		&JitOps::op_Rep_xxx,						// Rep_xxx 
		&JitOps::op_Rep_S,							// Rep_S 
		&JitOps::op_Reset,							// Reset 
		&JitOps::op_Rnd,							// Rnd 
		&JitOps::op_Rol,							// Rol 
		&JitOps::op_Ror,							// Ror 
		&JitOps::op_Rti,							// Rti 
		&JitOps::op_Rts,							// Rts 
		&JitOps::op_Sbc,							// Sbc 
		&JitOps::op_Stop,							// Stop 
		&JitOps::op_Sub_SD,							// Sub_SD 
		&JitOps::op_Sub_xx,							// Sub_xx 
		&JitOps::op_Sub_xxxx,						// Sub_xxxx 
		&JitOps::op_Subl,							// Subl 
		&JitOps::op_Subr,							// subr 
		&JitOps::op_Tcc_S1D1,						// Tcc_S1D1 
		&JitOps::op_Tcc_S1D1S2D2,					// Tcc_S1D1S2D2 
		&JitOps::op_Tcc_S2D2,						// Tcc_S2D2 
		&JitOps::op_Tfr,							// Tfr 
		&JitOps::op_Trap,							// Trap 
		&JitOps::op_Trapcc,							// Trapcc 
		&JitOps::op_Tst,							// Tst 
		&JitOps::op_Vsl,							// Vsl 
		&JitOps::op_Wait							// Wait 
	};

	JitOps::JitOps(JitBlock& _block, JitBlockRuntimeData& _brt, const FastInterruptMode _fastInterruptMode/* = FastInterruptMode::None*/)
	: m_block(_block)
	, m_blockRuntimeData(_brt)
	, m_opcodes(_block.dsp().opcodes())
	, m_dspRegs(_block.regs())
	, m_asm(_block.asm_())
	, m_ccrDirty(_block.regs().ccrDirtyFlags())
	, m_fastInterruptMode(_fastInterruptMode)
	{
	}

	void JitOps::emit(const TWord _pc)
	{
		TWord op;
		TWord opB;
		m_block.dsp().memory().getOpcode(_pc, op, opB);
		emit(_pc, op, opB);
	}

	void JitOps::emit(const TWord _pc, const TWord _op, const TWord _opB)
	{
		m_pcCurrentOp = _pc;
		m_opWordA = _op;
		m_opWordB = _opB;
		m_opSize = 1;

		if(!_op)
		{
			emit(Nop, 0);
			return;
		}

		if(Opcodes::isNonParallelOpcode(_op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(_op);

			if(!oi)
			{
				m_opcodes.findNonParallelOpcodeInfo(_op);		// retry here to help debugging
				assert(0 && "illegal instruction");
			}

			emit(oi->m_instruction, _op);
			return;
		}

		const auto* oiMove = m_opcodes.findParallelMoveOpcodeInfo(_op);
		if(!oiMove)
		{
			m_opcodes.findParallelMoveOpcodeInfo(_op);		// retry here to help debugging
			assert(0 && "illegal instruction");
		}

		const OpcodeInfo* oiAlu = nullptr;

		if(_op & 0xff)
		{
			oiAlu = m_opcodes.findParallelAluOpcodeInfo(_op);
			if(!oiAlu)
			{
				m_opcodes.findParallelAluOpcodeInfo(_op);	// retry here to help debugging
				assert(0 && "invalid instruction");						
			}
		}

		switch (oiMove->m_instruction)
		{
		case Move_Nop:
			// Only ALU, no parallel move
			if(oiAlu)
			{
				emit(oiAlu->m_instruction, _op);
			}
			else
			{
				op_Nop(_op);
			}
			break;
		case Ifcc:
		case Ifcc_U:
			// IFcc executes the ALU instruction if the condition is met, therefore no ALU exec by us
			if(oiAlu)
			{
				emit(oiMove->m_instruction, _op);
			}
			else
			{
				op_Nop(_op);						
			}
			break;
		default:
			if(!oiAlu)
			{
				// if there is no ALU instruction, do only the move
				emit(oiMove->m_instruction, _op);
			}
			else
			{
				// call special function that simulates latch registers for alu op + parallel move
				emit(oiMove->m_instruction, oiAlu->m_instruction, _op);
			}
		}
	}

	void JitOps::emit(const Instruction _inst, const TWord _op)
	{
		m_instruction = _inst;

		getRegisters(m_writtenRegs, m_readRegs, _inst, _op);

		m_block.dspRegPool().setIsParallelOp(false);

		emitOpProlog();

		const auto& func = g_opcodeFuncs[_inst];
		(this->*func)(_op);

		emitOpEpilog();
	}

	void JitOps::emit(const Instruction _instMove, const Instruction _instAlu, const TWord _op)
	{
		m_instruction = Parallel;

		const auto& funcMove = g_opcodeFuncs[_instMove];
		const auto& funcAlu = g_opcodeFuncs[_instAlu];

		getRegisters(m_writtenRegs, m_readRegs, _instMove, _op);

		RegisterMask written, read;
		getRegisters(written, read, _instAlu, _op);
		add(m_writtenRegs, written);
		add(m_readRegs, read);

		m_block.dspRegPool().setIsParallelOp(true);

		emitOpProlog();
	
		(this->*funcAlu)(_op);
		m_asm.nop();
		(this->*funcMove)(_op);

		m_block.dspRegPool().parallelOpEpilog();
		
		emitOpEpilog();
	}

	void JitOps::emitOpProlog()
	{
	}

	void JitOps::emitOpEpilog()
	{
	}

	uint32_t JitOps::calcCycles(const TWord _pc) const
	{
		TWord opA;
		TWord opB;
		m_block.dsp().memory().getOpcode(_pc, opA, opB);
		Instruction instA;
		Instruction instB;
		m_block.dsp().opcodes().getInstructionTypes(opA, instA, instB);

		return dsp56k::calcCycles(instA, instB, _pc, opA, m_block.dsp().memory().getBridgedMemoryAddress(), 1);
	}

	void JitOps::errNotImplemented(TWord op)
	{
		assert(0 && "instruction not implemented");
	}

	void JitOps::do_exec(const DspValue& _lc, TWord _addr)
	{
		auto startLoop = [&]()
		{
			{
				DspValue la(m_block), lc(m_block);
				m_dspRegs.getLA(la);
				m_dspRegs.getLC(lc);
				setSSHSSL(la, lc);
			}

			m_asm.mov(m_dspRegs.getLA(JitDspRegs::Write), asmjit::Imm(_addr));

			if(_lc.isImmediate())
				m_asm.mov(r32(m_dspRegs.getLC(JitDspRegs::Write)), asmjit::Imm(_lc.imm24()));
			else
				m_asm.mov(r32(m_dspRegs.getLC(JitDspRegs::Write)), _lc.get());

			pushPCSR();

			m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(SR_LF));
		};

		if(_lc.isImmediate())
		{
			if (_lc.imm24())
				startLoop();
			else
				jmp(_addr + 1);
			return;
		}

		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			m_asm.test_(_lc.get());
			m_asm.jz(_toFalse);
		}, startLoop
		, [&]()
		{
			jmp(_addr+1);
		});
	}

	void JitOps::do_end()
	{
		const RegGP r(m_block);
		do_end(r);
	}
	void JitOps::do_end(const RegGP& r)
	{
		// restore previous loop flag
		{
			m_dspRegs.getSS(r64(r.get()));
			m_asm.and_(r32(r), asmjit::Imm(SR_LF));
			m_asm.and_(r32(m_dspRegs.getSR(JitDspRegs::ReadWrite)), asmjit::Imm(~SR_LF));
			m_asm.or_(r32(m_dspRegs.getSR(JitDspRegs::ReadWrite)), r32(r.get()));
		}

		// decrement SP twice, restoring old loop settings
		decSP();

		{
			DspValue lc(m_block, PoolReg::DspLC, false, true);
			DspValue la(m_block, PoolReg::DspLA, false, true);

			m_dspRegs.getSS(r64(lc));

#ifdef HAVE_ARM64
			m_asm.ubfx(r64(la), r64(lc), asmjit::Imm(24), asmjit::Imm(24));
			m_asm.ubfx(r64(lc), r64(lc), asmjit::Imm(0), asmjit::Imm(24));
#else
			m_asm.ror(r64(la), r64(lc), 24);
			m_asm.and_(r32(la), asmjit::Imm(0xffffff));
			m_asm.and_(r32(lc), asmjit::Imm(0xffffff));
#endif
		}

		decSP();

		m_resultFlags |= WriteToLC | WriteToLA;

	//	LOG( "DO END: loop flag = " << sr_test(SR_LF) << " sc=" << (int)sc.var << " lc:" << std::hex << lc.var << " la:" << std::hex << la.var );
	}

	void JitOps::op_Stop(TWord op)
	{
#ifdef HAVE_X86_64
		m_block.asm_().int3();
#else
		m_block.asm_().brk(asmjit::Imm(m_pcCurrentOp & 0xffff));
#endif
	}

	void callDSPDebug(DSP* const _dsp, const TWord op)
	{
		_dsp->op_Debug(op);
	}

	void callDSPWait(DSP* const _dsp, const TWord op)
	{
		_dsp->op_Wait(op);
	}

	void JitOps::op_Debug(TWord op)
	{
		// make sure that the debugger sees all latest register values correctly
		m_block.dspRegPool().debugStoreAll();

		callDSPFunc(&callDSPDebug, op);
	}

	void JitOps::op_Debugcc(TWord op)
	{
		checkCondition<Debugcc>(op, [&]()
		{
			op_Debug(op);
		}, false);
	}

	void JitOps::op_Wait(TWord op)
	{
		// TODO use this for idle processng
		callDSPFunc(&callDSPWait, op);
		/*
#ifdef HAVE_X86_64
		m_asm.nop(ptr(r32(regDspPtr), static_cast<int32_t>(m_pcCurrentOp)));
#else
		m_asm.nop();
#endif
*/
	}

	void JitOps::op_Do_ea(TWord op)
	{
		const auto addr = absAddressExt<Do_ea>();
		DspValue lc(m_block);
		readMem<Do_ea>(lc, op);
		do_exec(lc, addr);
	}

	void JitOps::op_Do_aa(TWord op)
	{
		const auto addr = absAddressExt<Do_aa>();
		DspValue lc(m_block);
		readMem<Do_aa>(lc, op);
		do_exec(lc, addr);
	}

	void JitOps::op_Do_xxx(TWord op)
	{
		const TWord addr = absAddressExt<Do_xxx>();
		const TWord loopcount = getFieldValue<Do_xxx,Field_hhhh, Field_iiiiiiii>(op);

		DspValue lc(m_block, loopcount, DspValue::Immediate24);
		do_exec( lc, addr );
	}

	void JitOps::op_Do_S(TWord op)
	{
		const auto addr = absAddressExt<Do_S>();
		const auto dddddd = getFieldValue<Do_S,Field_DDDDDD>(op);

		DspValue lc(m_block);
		decode_dddddd_read(lc, dddddd);
		do_exec( lc, addr );
	}

	void JitOps::op_Dor_ea(TWord op)
	{
		DspValue lc(m_block);
		readMem<Dor_ea>(lc, op);
		const auto displacement = pcRelativeAddressExt<Dor_xxx>();
		do_exec(lc, m_pcCurrentOp + displacement);
	}

	void JitOps::op_Dor_xxx(TWord op)
	{
        const auto loopcount = getFieldValue<Dor_xxx,Field_hhhh, Field_iiiiiiii>(op);
        const auto displacement = pcRelativeAddressExt<Dor_xxx>();

        const DspValue lc(m_block, loopcount, DspValue::Immediate24);
        do_exec(lc, m_pcCurrentOp + displacement);
	}

	void JitOps::op_Dor_S(TWord op)
	{
		const auto dddddd = getFieldValue<Dor_S,Field_DDDDDD>(op);

		DspValue lc(m_block);
		decode_dddddd_read(lc, dddddd);
		
		const auto displacement = pcRelativeAddressExt<Dor_S>();

		do_exec( lc, m_pcCurrentOp + displacement);
	}

	void JitOps::op_Enddo(TWord op)
	{
		do_end();
	}

	template<bool BackupCCR> void JitOps::op_Ifcc(const TWord op)
	{
		// preload all registers that the alu op needs to ensure nothing is loaded/unloaded within the if block
		const auto* oiAlu = m_opcodes.findParallelAluOpcodeInfo(op);

		RegisterMask regsWritten;
		RegisterMask regsRead;

		getRegisters(regsWritten, regsRead, oiAlu->getInstruction(), op);

		// remove SR because we lock it anyway below
		reinterpret_cast<uint64_t&>(regsWritten) &= ~static_cast<uint64_t>(RegisterMask::SR);
		reinterpret_cast<uint64_t&>(regsRead) &= ~static_cast<uint64_t>(RegisterMask::SR);

		// we need to read registers that are write-only beause the write is conditional. Otherwise this would discard the current register values if the write is not happening and writes back crap to memory
		regsRead |= regsWritten;

		const TWord cccc = getFieldValue<Ifcc,Field_CCCC>(op);

		const DSPReg sr(m_block, PoolReg::DspSR, true, false);

		const auto lockedRegs = getBlock().dspRegPool().lock(regsRead, regsWritten);

		If(m_block, m_blockRuntimeData, [&](auto _toFalse)
		{
			const auto cc = decode_cccc(cccc);
#ifdef HAVE_ARM64
			m_asm.b(reverseCC(cc), _toFalse);
#else
			m_asm.j(reverseCC(cc), _toFalse);
#endif
		}, [&]()
		{
			auto emitAluOp = [&]
			{
				emit(oiAlu->getInstruction(), op);
			};

			if constexpr(BackupCCR)
			{
				// TODO: alus should not update the CCR so we don't have to use backup stuff
				const RegXMM ccrBackup(m_block);
				m_asm.movd(ccrBackup, r32(m_dspRegs.getSR(JitDspRegs::Read)));

				auto& dirty = m_dspRegs.ccrDirtyFlags();

				const auto dirtyBackup = dirty;

				m_disableCCRUpdates = true;
				emitAluOp();
				m_disableCCRUpdates = false;

				dirty = dirtyBackup;

				if(m_block.dspRegPool().isWritten(PoolReg::DspSR))
				{
					const RegGP r(m_block);
					m_asm.movd(r32(r.get()), ccrBackup);
#ifdef HAVE_ARM64
					m_asm.bfi(r32(m_dspRegs.getSR(JitDspRegs::ReadWrite)), r32(r), asmjit::Imm(0), asmjit::Imm(8));
#else
					m_asm.mov(m_dspRegs.getSR(JitDspRegs::ReadWrite).r8(), r.get().r8());
#endif
				}
			}
			else
			{
				emitAluOp();
			}
		}, [] {}, false, !BackupCCR, false);

		getBlock().dspRegPool().unlock(lockedRegs);
	}

	void JitOps::op_Lua_ea(const TWord _op)
	{
		const auto mm		= getFieldValue<Lua_ea,Field_MM>(_op);
		const auto rrr		= getFieldValue<Lua_ea,Field_RRR>(_op);
		const auto ddddd	= getFieldValue<Lua_ea,Field_ddddd>(_op);

		auto r = updateAddressRegister(mm, rrr, false, true);

		decode_dddddd_write( ddddd, r);
	}

	void JitOps::op_Lua_Rn(TWord op)
	{
		const auto dddd		= getFieldValue<Lua_Rn,Field_dddd>(op);
		const auto a		= getFieldValue<Lua_Rn,Field_aaa, Field_aaaa>(op);
		const auto rrr		= getFieldValue<Lua_Rn,Field_RRR>(op);

		const auto r = decode_RRR_read(rrr, signextend<int,7>(a));

		if( dddd < 8 )									// r0-r7
		{
			m_dspRegs.setR(dddd, r);
		}
		else
		{
			m_dspRegs.setN(dddd & 7, r);
		}
	}

	void callDSPPflushun(DSP* const _dsp, const TWord op)
	{
		_dsp->op_Pflushun(op);
	}

	void callDSPPfree(DSP* const _dsp, const TWord op)
	{
		_dsp->op_Pfree(op);
	}

	void callDSPPlock(DSP* const _dsp, const TWord ea)
	{
		_dsp->cachePlock(ea);
	}

	void JitOps::op_Pflushun(TWord op)
	{
		callDSPFunc(&callDSPPflushun, op);
	}

	void JitOps::op_Pfree(TWord op)
	{
		callDSPFunc(&callDSPPfree, op);
	}

	void JitOps::op_Plock(TWord op)
	{
		const auto ea = effectiveAddress<Plock>(op);
		if(ea.isImm24())
			callDSPFunc(&callDSPPlock, ea.imm24());
		else
			callDSPFunc(&callDSPPlock, r64(ea.get()));
	}

	void JitOps::jmp(const DspValue& _absAddr)
	{
		m_dspRegs.setPC(_absAddr);
	}

	void JitOps::jsr(const DspValue& _absAddr)
	{
		pushPCSR();
		jmp(_absAddr);

		if (m_fastInterruptMode != FastInterruptMode::None)
		{
			DspValue srVal(m_block, PoolReg::DspSR, true, true);
			const auto sr = r32(srVal);

			const SkipLabel skip(m_asm);

			if(m_fastInterruptMode == FastInterruptMode::Dynamic)
			{
				const RegGP processingMode(m_block);
				getDspProcessingMode(r64(processingMode));

				m_asm.cmp(processingMode, DSP::ProcessingMode::FastInterrupt);
				m_asm.jnz(skip);
			}

#ifdef HAVE_ARM64
			m_asm.and_(sr, asmjit::Imm(~(SR_S1 | SR_S0)));
			m_asm.and_(sr, asmjit::Imm(~(SR_SA)));
			m_asm.and_(sr, asmjit::Imm(~(SR_LF)));
#else
			m_asm.and_(sr, asmjit::Imm(~(SR_S1 | SR_S0 | SR_SA | SR_LF)));
#endif
			setDspProcessingMode(DSP::LongInterrupt);
		}
	}

	void JitOps::jmp(const TWord _absAddr)
	{
		DspValue r(m_block, _absAddr, DspValue::Immediate24);
		jmp(r);
	}

	void JitOps::jsr(const TWord _absAddr)
	{
		DspValue r(m_block, _absAddr, DspValue::Immediate24);
		jsr(r);
	}

	void JitOps::op_Rti(TWord op)
	{
		popPCSR();

		setDspProcessingMode(DSP::DefaultPreventInterrupt);
	}

	void JitOps::op_Rts(TWord op)
	{
		popPC();
	}

	void callDSPReset(DSP* const _dsp, const TWord op)
	{
		_dsp->op_Reset(op);
	}

	void JitOps::rep_exec(const TWord _lc)
	{
		// detect div loops and use custom code to speed them up

		TWord opA;
		TWord opB;
		m_block.dsp().mem.getOpcode(m_pcCurrentOp + 1, opA, opB);

		if(opA && !OpcodeInfo::isParallelOpcode(opA))
		{
			const auto* oi = m_block.dsp().opcodes().findNonParallelOpcodeInfo(opA);
			if(oi && oi->getInstruction() == Div)
			{
				op_Rep_Div(opA, _lc);
				m_opSize++;
				return;
			}
		}
		else if(!opA)
		{
			// rep nop => do nothing
			m_blockRuntimeData.getEncodedInstructionCount() += _lc;

			// Note: We use _lc minus one because the operation itself was already counted once in JitBlock.cpp, where it just counts every translated op already
			m_blockRuntimeData.getEncodedCycleCount() += (_lc-1) * dsp56k::calcCycles(Nop, m_pcCurrentOp, 0, 0, 0);
			m_opSize++;
			return;
		}

		// regular processing
		DspValue lc(m_block, _lc, DspValue::Immediate24);
		rep_exec(lc);
	}

	void JitOps::rep_exec(DspValue& _lc)
	{
		const auto hasImmediateOperand = _lc.isImmediate();
		TWord lcImmediateOperand = hasImmediateOperand ? _lc.imm24() : 0;

		// backup LC
		const RegXMM lcBackup(m_block);
		m_asm.movd(lcBackup, r32(m_dspRegs.getLC(JitDspRegs::Read)));

		if(hasImmediateOperand)
			m_asm.mov(r32(m_dspRegs.getLC(JitDspRegs::Write)), asmjit::Imm(_lc.imm24()));
		else
			m_asm.mov(r32(m_dspRegs.getLC(JitDspRegs::Write)), r32(_lc.get()));

		_lc.release();

		const auto opSize = m_opSize;				// remember old op size as it gets overwritten by the child instruction
		const auto pc = m_pcCurrentOp + m_opSize;

		const auto repBodyCycles = calcCycles(pc);

		if(hasImmediateOperand)
		{
			m_blockRuntimeData.getEncodedInstructionCount() += lcImmediateOperand;
			m_blockRuntimeData.getEncodedCycleCount() += (lcImmediateOperand-1) * repBodyCycles;
		}
		else
		{
			const auto lc = r32(m_dspRegs.getLC(JitDspRegs::Read));
			m_block.increaseInstructionCount(lc);

			const RegGP temp(m_block);
			m_asm.mov(r32(temp), asmjit::Imm(repBodyCycles));
#ifdef HAVE_ARM64
			m_asm.smull(r64(temp), r32(temp), r32(lc));
#else
			m_asm.imul(r32(temp), r32(lc));
#endif
			m_asm.sub(r32(temp), repBodyCycles);
			m_block.increaseCycleCount(r32(temp));
		}

		const auto loopCycle = [&](bool _compare = true, TWord compareValue = 0)
		{
			const auto oldPushedRegCount = m_block.stack().pushedRegCount();

			const auto prevLoaded = m_block.dspRegPool().getLoadedRegs();
			const auto prevSpilled = m_block.dspRegPool().getSpilledRegs();

			m_asm.dec(m_dspRegs.getLC(JitDspRegs::ReadWrite));
			emit(pc);

			if(_compare)
			{
				if(compareValue)
					m_asm.cmp(r32(m_dspRegs.getLC(JitDspRegs::Read)), asmjit::Imm(compareValue));
				else
					m_asm.test_(r32(m_dspRegs.getLC(JitDspRegs::Read)));
			}

			const auto newPushedRegCount = m_block.stack().pushedRegCount();

			if(m_repMode != RepFirst)
				assert(newPushedRegCount == oldPushedRegCount);

			if(m_repMode == RepLoop)
			{
				// this is not currently happening, but might be for other code in the future. Check it once it appears.
				// A valid safe bet /workaround is to release the whole pool after each loop iteration, but costs a lot of performance obviously
				const auto written = m_block.dspRegPool().getWrittenRegs();

				const auto nowLoaded = m_block.dspRegPool().getLoadedRegs();
				const auto nowSpilled = m_block.dspRegPool().getSpilledRegs();

				const auto prevNotLoaded = ~prevLoaded;
				const auto prevNotSpilled = ~prevSpilled;

				const auto loadedDuringCycle = prevNotLoaded & nowLoaded;
				const auto spilledDuringCycle = prevNotSpilled & nowSpilled;

				const auto loadedAndWritten = loadedDuringCycle & written;
				const auto spilledAndWritten = spilledDuringCycle & written;

				assert(loadedAndWritten == DspRegFlags::None);
				assert(spilledAndWritten == DspRegFlags::None);

				m_block.dspRegPool().releaseByFlags(loadedAndWritten);
				m_block.dspRegPool().releaseByFlags(spilledAndWritten);
			}
		};

		const auto start = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		if(hasImmediateOperand)
		{
			if(lcImmediateOperand == 0)
				lcImmediateOperand = 65536;

			// unrool up to a size of 4
			if(lcImmediateOperand <= 4)
			{
				for(TWord i=0; i<lcImmediateOperand; ++i)
				{
					if(i == lcImmediateOperand-1)
						m_repMode = RepLast;
					else if(i==0)
						m_repMode = RepFirst;
					else
						m_repMode = RepLoop;

					loopCycle(false);
				}
			}
			else
			{
				m_block.dspRegPool().releaseAll();
				m_block.dspRegPool().setRepMode(true);

				// execute it two times without being part of the loop to fill register cache
				m_repMode = RepFirst;
				loopCycle(false);

				m_repMode = RepLoop;
				loopCycle(false);

				m_asm.bind(start);
				loopCycle(true, 1);
				m_asm.jnz(start);

				m_repMode = RepLast;
				loopCycle(false);			
			}
		}
		else
		{
			const auto last = m_asm.newLabel();

			m_block.dspRegPool().releaseAll();
			m_block.dspRegPool().setRepMode(true);

			m_asm.test_(r32(m_dspRegs.getLC(JitDspRegs::Read)));
			m_asm.jz(end);

			// execute it two times without being part of the loop to fill register cache
			m_repMode = RepFirst;
			loopCycle(true, 1);
			m_asm.jz(last);
			m_asm.test_(r32(m_dspRegs.getLC(JitDspRegs::Read)));
			m_asm.jz(end);

			m_repMode = RepLoop;
			loopCycle(true, 1);
			m_asm.jz(last);

			m_asm.bind(start);
			loopCycle(true, 1);
			m_asm.jnz(start);

			m_repMode = RepLast;
			m_asm.bind(last);
			loopCycle(false);
		}

		m_asm.bind(end);

		m_block.dspRegPool().setRepMode(false);
		m_repMode = RepNone;
		m_repTemps.clear();

		// restore previous LC
		m_asm.movd(r32(m_dspRegs.getLC(JitDspRegs::Write)), lcBackup);

		// op size is the sum of the rep plus the child op
		assert(m_opSize == 1 && "repeated instruction needs to be a single word instruction");
		m_opSize += opSize;
	}

	void JitOps::op_Rep_xxx(TWord op)
	{
		const auto loopcount = getFieldValue<Rep_xxx,Field_hhhh, Field_iiiiiiii>(op);
		rep_exec(loopcount);
	}

	void JitOps::op_Rep_S(TWord op)
	{
		const auto dddddd = getFieldValue<Rep_S,Field_dddddd>(op);
		DspValue lc(m_block);
		decode_dddddd_read(lc, dddddd);
		rep_exec(lc);
	}

	void JitOps::op_Reset(TWord op)
	{
		callDSPFunc(&callDSPReset, op);
	}

	bool JitOps::isPeriphAddress(const TWord _addr) const
	{
		return _addr >= getPeriphStartAddr();
	}

	TWord JitOps::getPeriphStartAddr() const
	{
		const auto* mode = m_block.getMode();
		if(!mode)
		{
			assert(!m_block.getConfig().support16BitSCMode);
			return XIO_Reserved_High_First;
		}
		if(mode->testSR(SRB_SC))
			return XIO_Reserved_High_First_16;
		return XIO_Reserved_High_First;
	}

}
