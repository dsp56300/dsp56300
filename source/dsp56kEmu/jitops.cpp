#include "jitops.h"

#include "dsp.h"
#include "jit.h"
#include "jitblock.h"

#include "jitops_alu.inl"
#include "jitops_ccr.inl"
#include "jitops_helper.inl"

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
		&JitOps::op_Ifcc,							// Ifcc 
		&JitOps::op_Ifcc_U,							// Ifcc_U 
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
		&JitOps::op_Mac_S,							// Mac_S 
		&JitOps::op_Maci_xxxx,						// Maci_xxxx 
		&JitOps::op_Macsu,							// Macsu 
		&JitOps::op_Macr_S1S2,						// Macr_S1S2 
		&JitOps::op_Macr_S,							// Macr_S 
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
		&JitOps::op_Mpy_SD,							// Mpy_SD 
		&JitOps::op_Mpy_su,							// Mpy_su 
		&JitOps::op_Mpyi,							// Mpyi 
		&JitOps::op_Mpyr_S1S2D,						// Mpyr_S1S2D 
		&JitOps::op_Mpyr_SD,						// Mpyr_SD 
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
		&JitOps::op_subr,							// subr 
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

	JitOps::JitOps(JitBlock& _block, bool _useSRCache)
	: m_block(_block)
	, m_opcodes(_block.dsp().opcodes())
	, m_dspRegs(_block.regs())
	, m_asm(_block.asm_())
	, m_useCCRCache(_useSRCache)
	{
	}

	void JitOps::emit(TWord pc, TWord op)
	{
		m_pcCurrentOp = pc;

		if(Opcodes::isNonParallelOpcode(op))
		{
			const auto* oi = m_opcodes.findNonParallelOpcodeInfo(op);

			if(!oi)
			{
				m_opcodes.findNonParallelOpcodeInfo(op);		// retry here to help debugging
				assert(0 && "illegal instruction");
			}

			emit(oi->m_instruction, op);
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
				emit(oiAlu->m_instruction, op);
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
				emit(oiMove->m_instruction, op);
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
				emit(oiMove->m_instruction, op);
			}
			else
			{
				// call special function that simulates latch registers for alu op + parallel move
				emit(oiMove->m_instruction, oiAlu->m_instruction, op);
			}
		}
	}

	void JitOps::emit(const Instruction _inst, const TWord _op)
	{
		emitOpProlog();

		const auto& func = g_opcodeFuncs[_inst];
		(this->*func)(_op);

		emitOpEpilog();
	}

	void JitOps::emit(const Instruction _instMove, const Instruction _instAlu, const TWord _op)
	{
		const auto& funcMove = g_opcodeFuncs[_instMove];
		const auto& funcAlu = g_opcodeFuncs[_instAlu];

		emitOpProlog();

		// TODO: latch registers

		(this->*funcMove)(_op);
		(this->*funcAlu)(_op);

		emitOpEpilog();
	}

	void JitOps::emitOpProlog()
	{
		const auto label = m_asm.newLabel();
		m_asm.bind(label);

		m_pcLabels.insert(std::make_pair(m_pcCurrentOp, label));

		m_asm.inc(m_dspRegs.getPC());
	}

	void JitOps::emitOpEpilog()
	{
	}

	void JitOps::XYto56(const asmjit::x86::Gpq& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		signextend48to56(_dst);
	}

	void JitOps::XY0to56(const asmjit::x86::Gpq& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shl(_dst, asmjit::Imm(40));
		m_asm.sar(_dst, asmjit::Imm(16));
	}

	void JitOps::XY1to56(const asmjit::x86::Gpq& _dst, int _xy) const
	{
		m_dspRegs.getXY(_dst, _xy);
		m_asm.shr(_dst, asmjit::Imm(24));	// remove LSWord
		signed24To56(_dst);
	}
}
