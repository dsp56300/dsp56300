#pragma once

#include <map>

#include "jitdspregs.h"
#include "jitregtracker.h"
#include "opcodes.h"
#include "opcodetypes.h"
#include "registers.h"
#include "types.h"
#include "utils.h"

namespace dsp56k
{
	class Jit;
	class Opcodes;

	class JitOps
	{
	public:
		JitOps(JitBlock& _block, bool _useSRCache = true);

		void emit(TWord _pc);
		void emit(TWord _pc, TWord _op, TWord _opB = 0);
		void emit(Instruction _inst, TWord _op);
		void emit(Instruction _instMove, Instruction _instAlu, TWord _op);

		void emitOpProlog();
		void emitOpEpilog();

		void op_Abs(TWord op);
		void op_ADC(TWord op)			{ errNotImplemented(op); }
		void op_Add_SD(TWord op);
		void op_Add_xx(TWord op);
		void op_Add_xxxx(TWord op);
		void op_Addl(TWord op);
		void op_Addr(TWord op);
		void op_And_SD(TWord op);
		void op_And_xx(TWord op);
		void op_And_xxxx(TWord op);
		void op_Andi(TWord op);
		void op_Asl_D(TWord op);
		void op_Asl_ii(TWord op);
		void op_Asl_S1S2D(TWord op);
		void op_Asr_D(TWord op);
		void op_Asr_ii(TWord op);
		void op_Asr_S1S2D(TWord op);
		void op_Bcc_xxxx(TWord op){}
		void op_Bcc_xxx(TWord op){}
		void op_Bcc_Rn(TWord op){}
		void op_Bchg_ea(TWord op);
		void op_Bchg_aa(TWord op);
		void op_Bchg_pp(TWord op);
		void op_Bchg_qq(TWord op);
		void op_Bchg_D(TWord op);
		void op_Bclr_ea(TWord op);
		void op_Bclr_aa(TWord op);
		void op_Bclr_pp(TWord op);
		void op_Bclr_qq(TWord op);
		void op_Bclr_D(TWord op);
		void op_Bra_xxxx(TWord op){}
		void op_Bra_xxx(TWord op){}
		void op_Bra_Rn(TWord op){}
		void op_Brclr_ea(TWord op){}
		void op_Brclr_aa(TWord op){}
		void op_Brclr_pp(TWord op){}
		void op_Brclr_qq(TWord op){}
		void op_Brclr_S(TWord op){}
		void op_BRKcc(TWord op)			{ errNotImplemented(op); }
		void op_Brset_ea(TWord op){}
		void op_Brset_aa(TWord op){}
		void op_Brset_pp(TWord op){}
		void op_Brset_qq(TWord op){}
		void op_Brset_S(TWord op){}
		void op_BScc_xxxx(TWord op){}
		void op_BScc_xxx(TWord op){}
		void op_BScc_Rn(TWord op){}
		void op_Bsclr_ea(TWord op){}
		void op_Bsclr_aa(TWord op){}
		void op_Bsclr_pp(TWord op){}
		void op_Bsclr_qq(TWord op){}
		void op_Bsclr_S(TWord op){}
		void op_Bset_ea(TWord op);
		void op_Bset_aa(TWord op);
		void op_Bset_pp(TWord op);
		void op_Bset_qq(TWord op);
		void op_Bset_D(TWord op);
		void op_Bsr_xxxx(TWord op){}
		void op_Bsr_xxx(TWord op){}
		void op_Bsr_Rn(TWord op){}
		void op_Bsset_ea(TWord op){}
		void op_Bsset_aa(TWord op){}
		void op_Bsset_pp(TWord op){}
		void op_Bsset_qq(TWord op){}
		void op_Bsset_S(TWord op){}
		void op_Btst_ea(TWord op);
		void op_Btst_aa(TWord op);
		void op_Btst_pp(TWord op);
		void op_Btst_qq(TWord op);
		void op_Btst_D(TWord op);
		void op_Clb(TWord op)				{ errNotImplemented(op); }
		void op_Clr(TWord op);
		void op_Cmp_S1S2(TWord op);
		void op_Cmp_xxS2(TWord op);
		void op_Cmp_xxxxS2(TWord op);
		void op_Cmpm_S1S2(TWord op);
		void op_Cmpu_S1S2(TWord op)			{ errNotImplemented(op); }
		void op_Debug(TWord op)				{ errNotImplemented(op); }
		void op_Debugcc(TWord op)			{ errNotImplemented(op); }
		void op_Dec(TWord op);
		void op_Div(TWord op);
		void op_Dmac(TWord op);
		void op_Do_ea(TWord op){}
		void op_Do_aa(TWord op){}
		void op_Do_xxx(TWord op){}
		void op_Do_S(TWord op){}
		void op_DoForever(TWord op){}
		void op_Dor_ea(TWord op){}
		void op_Dor_aa(TWord op){}
		void op_Dor_xxx(TWord op){}
		void op_Dor_S(TWord op){}
		void op_DorForever(TWord op){}
		void op_Enddo(TWord op){}
		void op_Eor_SD(TWord op)			{ errNotImplemented(op); }
		void op_Eor_xx(TWord op)			{ errNotImplemented(op); }
		void op_Eor_xxxx(TWord op)			{ errNotImplemented(op); }
		void op_Extract_S1S2(TWord op)		{ errNotImplemented(op); }
		void op_Extract_CoS2(TWord op)		{ errNotImplemented(op); }
		void op_Extractu_S1S2(TWord op);
		void op_Extractu_CoS2(TWord op);
		template<bool BackupCCR> void op_Ifcc(TWord op);
		void op_Illegal(TWord op)			{ errNotImplemented(op); }
		void op_Inc(TWord op);
		void op_Insert_S1S2(TWord op)		{ errNotImplemented(op); }
		void op_Insert_CoS2(TWord op)		{ errNotImplemented(op); }
		void op_Jcc_xxx(TWord op){}
		void op_Jcc_ea(TWord op){}
		void op_Jclr_ea(TWord op){}
		void op_Jclr_aa(TWord op){}
		void op_Jclr_pp(TWord op){}
		void op_Jclr_qq(TWord op){}
		void op_Jclr_S(TWord op){}
		void op_Jmp_ea(TWord op){}
		void op_Jmp_xxx(TWord op){}
		void op_Jscc_xxx(TWord op){}
		void op_Jscc_ea(TWord op){}
		void op_Jsclr_ea(TWord op){}
		void op_Jsclr_aa(TWord op){}
		void op_Jsclr_pp(TWord op){}
		void op_Jsclr_qq(TWord op){}
		void op_Jsclr_S(TWord op){}
		void op_Jset_ea(TWord op){}
		void op_Jset_aa(TWord op){}
		void op_Jset_pp(TWord op){}
		void op_Jset_qq(TWord op){}
		void op_Jset_S(TWord op){}
		void op_Jsr_ea(TWord op){}
		void op_Jsr_xxx(TWord op){}
		void op_Jsset_ea(TWord op){}
		void op_Jsset_aa(TWord op){}
		void op_Jsset_pp(TWord op){}
		void op_Jsset_qq(TWord op){}
		void op_Jsset_S(TWord op){}
		void op_Lra_Rn(TWord op)		{ errNotImplemented(op); }
		void op_Lra_xxxx(TWord op);
		void op_Lsl_D(TWord op);
		void op_Lsl_ii(TWord op);
		void op_Lsl_SD(TWord op)		{ errNotImplemented(op); }
		void op_Lsr_D(TWord op);
		void op_Lsr_ii(TWord op);
		void op_Lsr_SD(TWord op)		{ errNotImplemented(op); }
		void op_Lua_ea(TWord _op);
		void op_Lua_Rn(TWord op);
		void op_Mac_S1S2(TWord op)		{ alu_multiply(op); }
		template<Instruction Inst, bool Accumulate, bool Round> void op_Mac_S(TWord op);
		void op_Maci_xxxx(TWord op)		{ errNotImplemented(op); }
		void op_Macsu(TWord op){}
		void op_Macr_S1S2(TWord op)		{ alu_multiply(op); }
//		void op_Macr_S(TWord op);
		void op_Macri_xxxx(TWord op)	{ errNotImplemented(op); }
		void op_Max(TWord op)			{ errNotImplemented(op); }
		void op_Maxm(TWord op)			{ errNotImplemented(op); }
		void op_Merge(TWord op)			{ errNotImplemented(op); }
		void op_Move_Nop(TWord op);
		void op_Move_xx(TWord op);
		void op_Mover(TWord op);
		void op_Move_ea(TWord op);
		void op_Movex_ea(TWord op);
		void op_Movex_aa(TWord op);
		void op_Movex_Rnxxxx(TWord op);
		void op_Movex_Rnxxx(TWord op);
		void op_Movexr_ea(TWord op);
		void op_Movexr_A(TWord op);
		void op_Movey_ea(TWord op);
		void op_Movey_aa(TWord op);
		void op_Movey_Rnxxxx(TWord op);
		void op_Movey_Rnxxx(TWord op);
		void op_Moveyr_ea(TWord op);
		void op_Moveyr_A(TWord op);
		void op_Movel_ea(TWord op);
		void op_Movel_aa(TWord op);
		void op_Movexy(TWord op);
		void op_Movec_ea(TWord op);
		void op_Movec_aa(TWord op);
		void op_Movec_S1D2(TWord op);
		void op_Movec_xx(TWord op);
		void op_Movem_ea(TWord op);
		void op_Movem_aa(TWord op)				{ errNotImplemented(op); }
		void op_Movep_ppea(TWord op);
		void op_Movep_Xqqea(TWord op);
		void op_Movep_Yqqea(TWord op);
		void op_Movep_eapp(TWord op)			{ errNotImplemented(op); }
		void op_Movep_eaqq(TWord op)			{ errNotImplemented(op); }
		void op_Movep_Spp(TWord op);
		void op_Movep_SXqq(TWord op);
		void op_Movep_SYqq(TWord op);
		void op_Mpy_S1S2D(TWord op)				{ alu_multiply(op); }
//		void op_Mpy_SD(TWord op);
		void op_Mpy_su(TWord op){}
		void op_Mpyi(TWord op){}
		void op_Mpyr_S1S2D(TWord op)			{ alu_multiply(op); }
//		void op_Mpyr_SD(TWord op)				{ errNotImplemented(op); }
		void op_Mpyri(TWord op)					{ errNotImplemented(op); }
		void op_Neg(TWord op);
		void op_Nop(TWord op);
		void op_Norm(TWord op)					{ errNotImplemented(op); }
		void op_Normf(TWord op)					{ errNotImplemented(op); }
		void op_Not(TWord op);
		void op_Or_SD(TWord op);
		void op_Or_xx(TWord op)					{ errNotImplemented(op); }
		void op_Or_xxxx(TWord op)				{ errNotImplemented(op); }
		void op_Ori(TWord op);
		void op_Pflush(TWord op){}
		void op_Pflushun(TWord op){}
		void op_Pfree(TWord op){}
		void op_Plock(TWord op){}
		void op_Plockr(TWord op){}
		void op_Punlock(TWord op){}
		void op_Punlockr(TWord op){}
		void op_Rep_ea(TWord op){}
		void op_Rep_aa(TWord op){}
		void op_Rep_xxx(TWord op){}
		void op_Rep_S(TWord op){}
		void op_Reset(TWord op){}
		void op_Rnd(TWord op);
		void op_Rol(TWord op);
		void op_Ror(TWord op)					{ errNotImplemented(op); }
		void op_Rti(TWord op){}
		void op_Rts(TWord op){}
		void op_Sbc(TWord op)					{ errNotImplemented(op); }
		void op_Stop(TWord op)					{ errNotImplemented(op); }
		void op_Sub_SD(TWord op);
		void op_Sub_xx(TWord op);
		void op_Sub_xxxx(TWord op);
		void op_Subl(TWord op)					{ errNotImplemented(op); }
		void op_Subr(TWord op)					{ errNotImplemented(op); }
		void op_Tcc_S1D1(TWord op){}
		void op_Tcc_S1D1S2D2(TWord op){}
		void op_Tcc_S2D2(TWord op){}
		void op_Tfr(TWord op){}
		void op_Trap(TWord op)					{ errNotImplemented(op); }
		void op_Trapcc(TWord op)				{ errNotImplemented(op); }
		void op_Tst(TWord op);
		void op_Vsl(TWord op)					{ errNotImplemented(op); }
		void op_Wait(TWord op)					{ errNotImplemented(op); }

		// helpers
		void signextend56to64(const JitReg64& _reg) const;
		void signextend48to64(const JitReg64& _reg) const;
		void signextend48to56(const JitReg64& _reg) const;
		void signextend24to56(const JitReg64& _reg) const;
		void signextend24to64(const JitReg64& _reg) const;

		void updateAddressRegister(const JitReg64& _r, TWord _mmm, TWord _rrr, bool _writeR = true, bool _returnPostR = false);
		void updateAddressRegister(const JitReg64& _r, const JitReg64& _n, const JitReg64& _m);
		void updateAddressRegisterModulo(const JitReg64& _r, const JitReg64& _n, const JitReg64& _m) const;
		void updateAddressRegisterMultipleWrapModulo(const JitReg64& _r, const JitReg64& _n, const JitReg64& _m);
		static void updateAddressRegisterBitreverse(const JitReg64& _r, const JitReg64& _n, const JitReg64& _m);

		void signed24To56(const JitReg64& _r) const;
		
		template<Instruction Inst> int pcRelativeAddressExt() const
		{
			static_assert(g_opcodes[Inst].m_extensionWordType & PCRelativeAddressExt, "opcode does not have a PC-relative address extension word");
			return signextend<int,24>(m_opWordB);
		}

		// DSP memory access
		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr> void effectiveAddress(const JitReg64& _dst, TWord _op);

		template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type* = nullptr> void readMem(const JitReg64& _dst, TWord _op);
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr> void readMem(const JitReg64& _dst, TWord _op, EMemArea _area);
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type* = nullptr> void readMem(const JitReg64& _dst, TWord op) const;
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type* = nullptr> void readMem(const JitReg64& _dst, TWord op) const;
		template <Instruction Inst, typename std::enable_if<!hasField<Inst, Field_s>() && hasFields<Inst, Field_aaaaaa, Field_S>()>::type* = nullptr> void readMem(const JitReg64& _dst, TWord op) const;
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_S, Field_s>() && hasField<Inst, Field_aaaaaa>()>::type* = nullptr> void readMem(const JitReg64& _dst, TWord op, EMemArea _area) const;

		template <Instruction Inst, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr> void writeMem(TWord _op, EMemArea _area, const JitReg64& _src);
		template <Instruction Inst, typename std::enable_if<!hasField<Inst, Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type* = nullptr> void writeMem(TWord _op, const JitReg64& _src);
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type* = nullptr> void writeMem(TWord op, const JitReg64& _src);
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type* = nullptr> void writeMem(TWord op, const JitReg64& _src);
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_S, Field_s>() && hasField<Inst, Field_aaaaaa>()>::type* = nullptr> void writeMem(TWord op, EMemArea _area, const JitReg64& _src) const;

		void readMemOrPeriph(const JitReg64& _dst, EMemArea _area, const JitReg64& _offset);
		void writeMemOrPeriph(EMemArea _area, const JitReg64& _offset, const JitReg64& _value);

		// DSP register access
		void getMR(const JitReg64& _dst) const;
		void getCCR(RegGP& _dst);
		void getCOM(const JitReg64& _dst) const;
		void getEOM(const JitReg64& _dst) const;

		void setMR(const JitReg64& _src) const;
		void setCCR(const JitReg64& _src);
		void setCOM(const JitReg64& _src) const;
		void setEOM(const JitReg64& _src) const;

		void getSR(const JitReg32& _dst);
		JitReg getSR();
		void setSR(const JitReg32& _src);

		void transferAluTo24(const JitReg& _dst, int _alu);
		void transfer24ToAlu(int _alu, const JitReg& _src);
		void transferSaturation(const JitReg& _dst);

		// CCR
		void ccr_update_ifZero(CCRBit _bit) const;
		void ccr_update_ifNotZero(CCRBit _bit) const;
		void ccr_update_ifGreater(CCRBit _bit) const;
		void ccr_update_ifGreaterEqual(CCRBit _bit) const;
		void ccr_update_ifLess(CCRBit _bit) const;
		void ccr_update_ifLessEqual(CCRBit _bit) const;
		void ccr_update_ifCarry(CCRBit _bit) const;
		void ccr_update_ifNotCarry(CCRBit _bit) const;
		void ccr_update_ifParity(CCRBit _bit) const;
		void ccr_update_ifNotParity(CCRBit _bit) const;
		void ccr_update_ifAbove(CCRBit _bit) const;
		void ccr_update_ifBelow(CCRBit _bit) const;

		void ccr_update(const RegGP& _value, CCRBit _bit) const;

		void ccr_u_update(const JitReg64& _alu) const;
		void ccr_e_update(const JitReg64& _alu) const;
		void ccr_n_update_by55(const JitReg64& _alu) const;
		void ccr_n_update_by47(const JitReg64& _alu) const;
		void ccr_n_update_by23(const JitReg64& _alu) const;
		void ccr_s_update(const JitReg64& _alu) const;
		void ccr_l_update_by_v() const;
		void ccr_v_update(const JitReg64& _nonMaskedResult) const;

		void ccr_clear(CCRMask _mask) const;
		void ccr_set(CCRMask _mask) const;
		void ccr_dirty(const JitReg64& _alu);
		void updateDirtyCCR();
		void updateDirtyCCR(const JitReg64& _alu);

		void sr_getBitValue(const JitReg& _dst, CCRBit _bit) const;

		void XYto56(const JitReg64& _dst, int _xy) const;
		void XY0to56(const JitReg64& _dst, int _xy) const;
		void XY1to56(const JitReg64& _dst, int _xy) const;

		// decode
		void decode_cccc(const JitReg& _dst, TWord cccc);
		void decode_dddddd_read(const JitReg32& _dst, TWord _dddddd);
		void decode_dddddd_write(TWord _dddddd, const JitReg32& _src);
		void decode_ddddd_pcr_read(const JitReg32& _dst, TWord _ddddd);
		void decode_ddddd_pcr_write(TWord _ddddd, const JitReg32& _src);
		void decode_ee_read(const JitReg& _dst, TWord _ee);
		void decode_ee_write(TWord _ee, const JitReg& _value);
		void decode_EE_read(RegGP& dst, TWord _ee);
		void decode_EE_write(const JitReg64& _src, TWord _ee);
		void decode_ff_read(const JitReg& _dst, TWord _ff);
		void decode_ff_write(TWord _ff, const JitReg& _value);
		void decode_JJJ_read_56(JitReg64 _dst, TWord JJJ, bool _b) const;
		void decode_JJ_read(JitReg64 _dst, TWord jj) const;
		void decode_RRR_read(const JitReg& _dst, TWord _mmmrrr, int _shortDisplacement = 0);
		void decode_qq_read(const JitReg& _dst, TWord _qq);
		void decode_QQ_read(const JitReg& _dst, TWord _qq);
		void decode_QQQQ_read(const JitReg& _s1, const JitReg& _s2, TWord _qqqq) const;
		void decode_sss_read(JitReg64 _dst, TWord _sss) const;
		void decode_LLL_read(TWord _lll, const JitReg32& x, const JitReg32& y);
		void decode_LLL_write(TWord _lll, const JitReg32& x, const JitReg32& y);
		void decode_XMove_MMRRR(const JitReg64& _dst, TWord _mm, TWord _rrr);
		
		TWord getOpWordB() const			{ return m_opWordB; }
		void getOpWordB(const JitReg& _dst) const;

		// ALU
		void unsignedImmediateToAlu(const RegGP& _r, const asmjit::Imm& _i) const;

		void alu_abs(const JitReg& _r) const;
		
		void alu_add(TWord _ab, RegGP& _v);
		void alu_add(TWord _ab, const asmjit::Imm& _v);

		void alu_sub(TWord _ab, RegGP& _v);
		void alu_sub(TWord _ab, const asmjit::Imm& _v);

		void alu_and(TWord ab, RegGP& _v) const;

		void alu_asl(TWord abSrc, TWord abDst, const PushGP& _v);
		void alu_asr(TWord _abSrc, TWord _abDst, const PushGP& _v);

		void alu_bclr(const JitReg64& _dst, TWord _bit) const;
		void alu_bset(const JitReg64& _dst, TWord _bit) const;
		void alu_bchg(const JitReg64& _dst, TWord _bit) const;

		void alu_cmp(TWord ab, const JitReg64& _v, bool magnitude);

		void alu_lsl(TWord ab, int _shiftAmount) const;
		void alu_lsr(TWord ab, int _shiftAmount) const;
		void alu_mpy(TWord ab, RegGP& _s1, RegGP& _s2, bool _negate, bool _accumulate, bool _s1Unsigned = false, bool _s2Unsigned = false);
		void alu_multiply(TWord op);
		void alu_or(TWord ab, const JitReg& _v);
		void alu_rnd(TWord ab);
		
		template<Instruction Inst> void bitmod_ea(TWord _op, void(JitOps::*_bitmodFunc)(const JitReg64&, TWord) const);
		template<Instruction Inst> void bitmod_aa(TWord _op, void(JitOps::*_bitmodFunc)(const JitReg64&, TWord) const);
		template<Instruction Inst> void bitmod_ppqq(TWord _op, void(JitOps::*_bitmodFunc)(const JitReg64&, TWord) const);
		template<Instruction Inst> void bitmod_D(TWord _op, void(JitOps::*_bitmodFunc)(const JitReg64&, TWord) const);

		// move
		template<Instruction Inst> void move_ddddd_MMMRRR(TWord _op, EMemArea _area);
		template<Instruction Inst> void move_ddddd_absAddr(TWord _op, EMemArea _area);
		template<Instruction Inst> void move_Rnxxxx(TWord op, EMemArea _area);
		template<Instruction Inst> void move_Rnxxx(TWord op, EMemArea _area);
		template<Instruction Inst> void move_L(TWord op);
		template<Instruction Inst> void movep_qqea(TWord op, EMemArea _area);
		template<Instruction Inst> void movep_sqq(TWord op, EMemArea _area);
	private:
		void errNotImplemented(TWord op);

		JitBlock& m_block;
		const Opcodes& m_opcodes;
		JitDspRegs& m_dspRegs;
		asmjit::x86::Assembler& m_asm;
		std::map<TWord, asmjit::Label> m_pcLabels;

		bool m_ccrDirty = false;
		const bool m_useCCRCache;

		TWord m_pcCurrentOp = 0;
		TWord m_opWordA = 0;
		TWord m_opWordB = 0;
	};
}
