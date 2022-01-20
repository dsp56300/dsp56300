#include "jitops.h"

#include "dsp.h"
#include "jit.h"
#include "jitblock.h"
#include "jithelper.h"

#include "jitops_alu.inl"
#include "jitops_ccr.inl"
#include "jitops_decode.inl"
#include "jitops_helper.inl"
#include "jitops_jmp.inl"
#include "jitops_mem.inl"
#include "jitops_move.inl"

#ifdef HAVE_ARM64
#include "jitops_aarch64.inl"
#include "jitops_alu_aarch64.inl"
#include "jitops_ccr_aarch64.inl"
#include "jitops_decode_aarch64.inl"
#include "jitops_helper_aarch64.inl"
#else
#include "jitops_alu_x64.inl"
#include "jitops_ccr_x64.inl"
#include "jitops_decode_x64.inl"
#include "jitops_helper_x64.inl"
#include "jitops_x64.inl"
#endif

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

	JitOps::JitOps(JitBlock& _block, bool _fastInterrupt/* = false*/)
	: m_block(_block)
	, m_opcodes(_block.dsp().opcodes())
	, m_dspRegs(_block.regs())
	, m_asm(_block.asm_())
	, m_ccrDirty(_block.regs().ccrDirtyFlags())
	, m_fastInterrupt(_fastInterrupt)
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

	inline void JitOps::errNotImplemented(TWord op)
	{
		assert(0 && "instruction not implemented");
	}

	inline void JitOps::do_exec(RegGP& _lc, TWord _addr)
	{
		If(m_block, [&](auto _toFalse)
		{
			m_asm.cmp(_lc, asmjit::Imm(0));
			m_asm.jz(_toFalse);
		}, [&]()
		{
			setSSHSSL(r32(m_dspRegs.getLA(JitDspRegs::Read)), r32(m_dspRegs.getLC(JitDspRegs::Read)));

			m_asm.mov(m_dspRegs.getLA(JitDspRegs::Write), asmjit::Imm(_addr));
			m_asm.mov(m_dspRegs.getLC(JitDspRegs::Write), _lc.get());

			_lc.release();

			pushPCSR();

			m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(SR_LF));
		}, [&]()
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
			m_asm.and_(r64(r), asmjit::Imm(SR_LF));
			m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(~SR_LF));
			m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), r.get());
		}

		// decrement SP twice, restoring old loop settings
		decSP();

		getSSL(r32(r.get()));
		m_dspRegs.setLC(r32(r.get()));

		getSSH(r32(r.get()));
		m_dspRegs.setLA(r32(r.get()));

		m_resultFlags |= WriteToLC | WriteToLA;

	//	LOG( "DO END: loop flag = " << sr_test(SR_LF) << " sc=" << (int)sc.var << " lc:" << std::hex << lc.var << " la:" << std::hex << la.var );
	}

	inline void JitOps::op_Debug(TWord op)
	{
	}

	inline void JitOps::op_Debugcc(TWord op)
	{
	}

	void JitOps::op_Do_ea(TWord op)
	{
		const auto addr = absAddressExt<Do_ea>();
		RegGP lc(m_block);
		readMem<Do_ea>(lc, op);
		do_exec(lc, addr);
	}

	void JitOps::op_Do_aa(TWord op)
	{
		const auto addr = absAddressExt<Do_aa>();
		RegGP lc(m_block);
		readMem<Do_aa>(lc, op);
		do_exec(lc, addr);
	}

	void JitOps::op_Do_xxx(TWord op)
	{
		const TWord addr = absAddressExt<Do_xxx>();
		const TWord loopcount = getFieldValue<Do_xxx,Field_hhhh, Field_iiiiiiii>(op);

        RegGP lc(m_block);
		m_asm.mov(lc, asmjit::Imm(loopcount));

		do_exec( lc, addr );
	}

	void JitOps::op_Do_S(TWord op)
	{
		const auto addr = absAddressExt<Do_S>();
		const auto dddddd = getFieldValue<Do_S,Field_DDDDDD>(op);

		RegGP lc(m_block);
		decode_dddddd_read(r32(lc.get()), dddddd );

		do_exec( lc, addr );
	}

	void JitOps::op_Dor_ea(TWord op)
	{
		RegGP lc(m_block);
		readMem<Dor_ea>(lc, op);
		const auto displacement = pcRelativeAddressExt<Dor_xxx>();
		do_exec(lc, m_pcCurrentOp + displacement);
	}

	void JitOps::op_Dor_xxx(TWord op)
	{
        const auto loopcount = getFieldValue<Dor_xxx,Field_hhhh, Field_iiiiiiii>(op);
        const auto displacement = pcRelativeAddressExt<Dor_xxx>();

        RegGP lc(m_block);
		m_asm.mov(lc, asmjit::Imm(loopcount));
		
        do_exec(lc, m_pcCurrentOp + displacement);
	}

	void JitOps::op_Dor_S(TWord op)
	{
		const auto dddddd = getFieldValue<Dor_S,Field_DDDDDD>(op);

		RegGP lc(m_block);
		decode_dddddd_read(r32(lc.get()), dddddd );
		
		const auto displacement = pcRelativeAddressExt<Dor_S>();

		do_exec( lc, m_pcCurrentOp + displacement);
	}

	void JitOps::op_Enddo(TWord op)
	{
		do_end();
	}

	template<bool BackupCCR> void JitOps::op_Ifcc(TWord op)
	{
		const TWord cccc = getFieldValue<Ifcc,Field_CCCC>(op);

		updateDirtyCCR();

		If(m_block, [&](auto _toFalse)
		{
			const RegGP test(m_block);
#ifdef HAVE_ARM64
			m_asm.mov(test, asmjit::a64::xzr);
			decode_cccc(test, cccc);
			m_asm.cmp(test.get(), asmjit::Imm(1));
			m_asm.cond_not_equal().b(_toFalse);
#else
			decode_cccc(test, cccc);

			m_asm.cmp(test.get().r8(), asmjit::Imm(1));
			m_asm.jne(_toFalse);	
#endif
		}, [&]()
		{
			auto emitAluOp = [&](const TWord _op)
			{
				const auto* oiAlu = m_opcodes.findParallelAluOpcodeInfo(_op);
				emit(oiAlu->getInstruction(), _op);
			};

			if constexpr(BackupCCR)
			{
				// TODO: alus should not update the CCR so we don't have to use backup stuff
				const RegXMM ccrBackup(m_block);
				m_asm.movd(ccrBackup, r32(getSR(JitDspRegs::Read)));

				emitAluOp(op);

				updateDirtyCCR();

				const RegGP r(m_block);
				m_asm.movd(r32(r.get()), ccrBackup);
				m_asm.and_(r, asmjit::Imm(0xff));
				m_asm.and_(m_dspRegs.getSR(JitDspRegs::ReadWrite), asmjit::Imm(0xffff00));
				m_asm.or_(m_dspRegs.getSR(JitDspRegs::ReadWrite), r.get());
			}
			else
			{
				emitAluOp(op);
			}
		});
	}

	inline void JitOps::op_Lua_ea(const TWord _op)
	{
		const auto mm		= getFieldValue<Lua_ea,Field_MM>(_op);
		const auto rrr		= getFieldValue<Lua_ea,Field_RRR>(_op);
		const auto ddddd	= getFieldValue<Lua_ea,Field_ddddd>(_op);

		const RegGP r(m_block);

		updateAddressRegister( r, mm, rrr, false, true);

		decode_dddddd_write( ddddd, r32(r.get()));
	}

	void JitOps::op_Lua_Rn(TWord op)
	{
		const auto dddd		= getFieldValue<Lua_Rn,Field_dddd>(op);
		const auto a		= getFieldValue<Lua_Rn,Field_aaa, Field_aaaa>(op);
		const auto rrr		= getFieldValue<Lua_Rn,Field_RRR>(op);

		const auto aSigned = signextend<int,7>(a);

		const RegGP r(m_block);
		m_dspRegs.getR(r, rrr);

		const RegGP n(m_block);
		m_asm.mov(n, asmjit::Imm(aSigned));

		const AguRegM m(m_block, rrr, true);

		updateAddressRegister(r32(r.get()), r32(n.get()), r32(m.get()), rrr);

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

	inline void JitOps::op_Pflushun(TWord op)
	{
		callDSPFunc(&callDSPPflushun, op);
	}

	inline void JitOps::op_Pfree(TWord op)
	{
		callDSPFunc(&callDSPPfree, op);
	}

	inline void JitOps::op_Plock(TWord op)
	{
		RegGP r(m_block);
		effectiveAddress<Plock>(r, op);
		callDSPFunc(&callDSPPlock, r);
	}

	inline void JitOps::jmp(const JitReg32& _absAddr)
	{
		m_dspRegs.setPC(_absAddr);
	}

	inline void JitOps::jsr(const JitReg32& _absAddr)
	{
		pushPCSR();
		jmp(_absAddr);

		if (m_fastInterrupt)
		{
			const auto sr = m_dspRegs.getSR(JitDspRegs::ReadWrite);
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

	inline void JitOps::jmp(TWord _absAddr)
	{
		const RegGP r(m_block);
		m_asm.mov(r, asmjit::Imm(_absAddr));
		jmp(r32(r.get()));
	}

	inline void JitOps::jsr(const TWord _absAddr)
	{
		const RegGP r(m_block);
		m_asm.mov(r32(r.get()), asmjit::Imm(_absAddr));
		jsr(r32(r.get()));
	}

	inline void JitOps::op_Rti(TWord op)
	{
		popPCSR();

		setDspProcessingMode(DSP::DefaultPreventInterrupt);
	}

	inline void JitOps::op_Rts(TWord op)
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

		// regular processing
		RegGP lc(m_block);
		m_asm.mov(lc, asmjit::Imm(_lc));
		rep_exec(lc, _lc);
	}

	void JitOps::rep_exec(RegGP& _lc, TWord _lcImmediateOperand)
	{
		const auto hasImmediateOperand = _lcImmediateOperand != std::numeric_limits<TWord>::max();

		// backup LC
		const RegXMM lcBackup(m_block);
		m_asm.movd(lcBackup, r32(m_dspRegs.getLC(JitDspRegs::Read)));
		m_asm.mov(r32(m_dspRegs.getLC(JitDspRegs::Write)), r32(_lc.get()));
		_lc.release();

		if(hasImmediateOperand)
		{
			m_block.getEncodedInstructionCount() += _lcImmediateOperand;
		}
		else
		{
			const auto lc = r32(m_dspRegs.getLC(JitDspRegs::Read));

			m_block.increaseInstructionCount(lc);
		}

		const auto opSize = m_opSize;
		const auto pc = m_pcCurrentOp + m_opSize;	// remember old op size as it gets overwritten by the child instruction
		
		const auto loopCycle = [&](bool _compare = true, TWord compareValue = 0)
		{
			m_asm.dec(m_dspRegs.getLC(JitDspRegs::ReadWrite));
			emit(pc);
			if(_compare)
				m_asm.cmp(m_dspRegs.getLC(JitDspRegs::Read), asmjit::Imm(compareValue));
		};

		const auto start = m_asm.newLabel();
		const auto end = m_asm.newLabel();

		m_block.dspRegPool().releaseAll();
		m_block.dspRegPool().setRepMode(true);

		if(hasImmediateOperand)
		{
			if(_lcImmediateOperand == 0)
				_lcImmediateOperand = 65536;

			if(_lcImmediateOperand == 1)
			{
				m_repMode = RepLast;
				loopCycle(false);
			}
			else if(_lcImmediateOperand == 2)
			{
				m_repMode = RepFirst;
				loopCycle(false);
				m_repMode = RepLast;
				loopCycle(false);
			}
			else
			{
				m_repMode = RepFirst;
				loopCycle(false);

				m_repMode = RepLoop;

				m_asm.bind(start);
				loopCycle(true, 1);
				m_asm.jnz(start);

				m_repMode = RepLast;
				loopCycle(false);			
			}
		}
		else
		{
			m_repMode = RepDynamic;

			// execute it once without being part of the loop to fill register cache
			loopCycle();
			m_asm.jz(end);

			m_asm.bind(start);
			loopCycle();
			m_asm.jnz(start);
		}

		m_asm.bind(end);

		m_block.dspRegPool().setRepMode(false);
		m_repMode = RepNone;

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
		RegGP lc(m_block);
		decode_dddddd_read(r32(lc.get()), dddddd);
		rep_exec(lc, std::numeric_limits<TWord>::max());
	}

	inline void JitOps::op_Reset(TWord op)
	{
		callDSPFunc(&callDSPReset, op);
	}
}
