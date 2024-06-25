#pragma once

#include "jitblock.h"
#include "jitdspregs.h"
#include "jitdspvalue.h"
#include "jitemitter.h"
#include "jitregtracker.h"
#include "jittypes.h"

#include "opcodes.h"
#include "opcodetypes.h"
#include "registers.h"
#include "types.h"
#include "utils.h"
#include "opcodeanalysis.h"

namespace dsp56k
{
	class Jit;
	class Opcodes;

	class JitOps
	{
	public:
		enum ResultFlags
		{
			None				= 0,

			WritePMem			= 0x01,
			WriteToLC			= 0x02,
			WriteToLA			= 0x04,
			PopPC				= 0x08
		};

		JitOps(JitBlock& _block, JitBlockRuntimeData& _brt, bool _fastInterrupt = false);

		void emit(TWord _pc);
		void emit(TWord _pc, TWord _op, TWord _opB = 0);
		void emit(Instruction _inst, TWord _op);
		void emit(Instruction _instMove, Instruction _instAlu, TWord _op);

		void emitOpProlog();
		void emitOpEpilog();

		uint32_t calcCycles(TWord _pc) const;

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
		void op_Bcc_xxxx(TWord op);
		void op_Bcc_xxx(TWord op);
		void op_Bcc_Rn(TWord op);
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
		void op_Bra_xxxx(TWord op);
		void op_Bra_xxx(TWord op);
		void op_Bra_Rn(TWord op);
		void op_Brclr_ea(TWord op);
		void op_Brclr_aa(TWord op);
		void op_Brclr_pp(TWord op);
		void op_Brclr_qq(TWord op);
		void op_Brclr_S(TWord op);
		void op_BRKcc(TWord op)			{ errNotImplemented(op); }
		void op_Brset_ea(TWord op);
		void op_Brset_aa(TWord op);
		void op_Brset_pp(TWord op);
		void op_Brset_qq(TWord op);
		void op_Brset_S(TWord op);
		void op_BScc_xxxx(TWord op);
		void op_BScc_xxx(TWord op);
		void op_BScc_Rn(TWord op);
		void op_Bsclr_ea(TWord op);
		void op_Bsclr_aa(TWord op);
		void op_Bsclr_pp(TWord op);
		void op_Bsclr_qq(TWord op);
		void op_Bsclr_S(TWord op);
		void op_Bset_ea(TWord op);
		void op_Bset_aa(TWord op);
		void op_Bset_pp(TWord op);
		void op_Bset_qq(TWord op);
		void op_Bset_D(TWord op);
		void op_Bsr_xxxx(TWord op);
		void op_Bsr_xxx(TWord op);
		void op_Bsr_Rn(TWord op);
		void op_Bsset_ea(TWord op);
		void op_Bsset_aa(TWord op);
		void op_Bsset_pp(TWord op);
		void op_Bsset_qq(TWord op);
		void op_Bsset_S(TWord op);
		void op_Btst_ea(TWord op);
		void op_Btst_aa(TWord op);
		void op_Btst_pp(TWord op);
		void op_Btst_qq(TWord op);
		void op_Btst_D(TWord op);
		void op_Clb(TWord op);
		void op_Clr(TWord op);
		void op_Cmp_S1S2(TWord op);
		void op_Cmp_xxS2(TWord op);
		void op_Cmp_xxxxS2(TWord op);
		void op_Cmpm_S1S2(TWord op);
		void op_Cmpu_S1S2(TWord op)			{ errNotImplemented(op); }
		void op_Debug(TWord op);
		void op_Debugcc(TWord op);
		void op_Dec(TWord op);
		void op_Div(TWord op);
		void op_Rep_Div(TWord _op, TWord _iterationCount);
		void op_Dmac(TWord op);
		void op_Do_ea(TWord op);
		void op_Do_aa(TWord op);
		void op_Do_xxx(TWord op);
		void op_Do_S(TWord op);
		void op_DoForever(TWord op)			{ errNotImplemented(op); }
		void op_Dor_ea(TWord op);
		void op_Dor_aa(TWord op)			{ errNotImplemented(op); }
		void op_Dor_xxx(TWord op);
		void op_Dor_S(TWord op);
		void op_DorForever(TWord op)		{ errNotImplemented(op); }
		void op_Enddo(TWord op);
		void op_Eor_SD(TWord op);
		void op_Eor_xx(TWord op);
		void op_Eor_xxxx(TWord op)			{ errNotImplemented(op); }
		void op_Extract_S1S2(TWord op)		{ errNotImplemented(op); }
		void op_Extract_CoS2(TWord op)		{ errNotImplemented(op); }
		void op_Extractu_S1S2(TWord op);
		void op_Extractu_CoS2(TWord op);
		template<bool BackupCCR> void op_Ifcc(TWord op);
		void op_Illegal(TWord op)			{ errNotImplemented(op); }
		void op_Inc(TWord op);
		void op_Insert_S1S2(TWord op);
		void op_Insert_CoS2(TWord op);
		void op_Jcc_xxx(TWord op);
		void op_Jcc_ea(TWord op);
		void op_Jclr_ea(TWord op);
		void op_Jclr_aa(TWord op);
		void op_Jclr_pp(TWord op);
		void op_Jclr_qq(TWord op);
		void op_Jclr_S(TWord op);
		void op_Jmp_ea(TWord op);
		void op_Jmp_xxx(TWord op);
		void op_Jscc_xxx(TWord op);
		void op_Jscc_ea(TWord op);
		void op_Jsclr_ea(TWord op);
		void op_Jsclr_aa(TWord op);
		void op_Jsclr_pp(TWord op);
		void op_Jsclr_qq(TWord op);
		void op_Jsclr_S(TWord op);
		void op_Jset_ea(TWord op);
		void op_Jset_aa(TWord op);
		void op_Jset_pp(TWord op);
		void op_Jset_qq(TWord op);
		void op_Jset_S(TWord op);
		void op_Jsr_ea(TWord op);
		void op_Jsr_xxx(TWord op);
		void op_Jsset_ea(TWord op);
		void op_Jsset_aa(TWord op);
		void op_Jsset_pp(TWord op);
		void op_Jsset_qq(TWord op);
		void op_Jsset_S(TWord op);
		void op_Lra_Rn(TWord op)		{ errNotImplemented(op); }
		void op_Lra_xxxx(TWord op);
		void op_Lsl_D(TWord op);
		void op_Lsl_ii(TWord op);
		void op_Lsl_SD(TWord op);
		void op_Lsr_D(TWord op);
		void op_Lsr_ii(TWord op);
		void op_Lsr_SD(TWord op);
		void op_Lua_ea(TWord _op);
		void op_Lua_Rn(TWord op);
		void op_Mac_S1S2(TWord op)		{ alu_multiply(op); }
		template<Instruction Inst, bool Accumulate, bool Round> void op_Mac_S(TWord op);
		void op_Maci_xxxx(TWord op)		{ errNotImplemented(op); }
		void op_Macr_S1S2(TWord op)		{ alu_multiply(op); }
		void op_Macri_xxxx(TWord op)	{ errNotImplemented(op); }
		void op_Max(TWord op);
		void op_Maxm(TWord op);
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
		void op_Movep_eapp(TWord op);
		void op_Movep_eaqq(TWord op)			{ errNotImplemented(op); }
		void op_Movep_Spp(TWord op);
		void op_Movep_SXqq(TWord op);
		void op_Movep_SYqq(TWord op);
		void op_Mpy_S1S2D(TWord op)				{ alu_multiply(op); }
		template<Instruction Inst, bool Accumulate> void op_Mpy_su(TWord op);
		void op_Mpyi(TWord op);
		void op_Mpyr_S1S2D(TWord op)			{ alu_multiply(op); }
		void op_Mpyri(TWord op)					{ errNotImplemented(op); }
		void op_Neg(TWord op);
		void op_Nop(TWord op);
		void op_Norm(TWord op)					{ errNotImplemented(op); }
		void op_Normf(TWord op);
		void op_Not(TWord op);
		void op_Or_SD(TWord op);
		void op_Or_xx(TWord op);
		void op_Or_xxxx(TWord op);
		void op_Ori(TWord op);
		void op_Pflush(TWord op)				{ errNotImplemented(op); }
		void op_Pflushun(TWord op);
		void op_Pfree(TWord op);
		void op_Plock(TWord op);
		void op_Plockr(TWord op)				{ errNotImplemented(op); }
		void op_Punlock(TWord op)				{ errNotImplemented(op); }
		void op_Punlockr(TWord op)				{ errNotImplemented(op); }
		void op_Rep_ea(TWord op)				{ errNotImplemented(op); }
		void op_Rep_aa(TWord op)				{ errNotImplemented(op); }
		void op_Rep_xxx(TWord op);
		void op_Rep_S(TWord op);
		void op_Reset(TWord op);
		void op_Rnd(TWord op);
		void op_Rol(TWord op);
		void op_Ror(TWord op)					{ errNotImplemented(op); }
		void op_Rti(TWord op);
		void op_Rts(TWord op);
		void op_Sbc(TWord op)					{ errNotImplemented(op); }
		void op_Stop(TWord op);
		void op_Sub_SD(TWord op);
		void op_Sub_xx(TWord op);
		void op_Sub_xxxx(TWord op);
		void op_Subl(TWord op);
		void op_Subr(TWord op);
		void op_Tcc_S1D1(TWord op);
		void op_Tcc_S1D1S2D2(TWord op);
		void op_Tcc_S2D2(TWord op);
		void op_Tfr(TWord op);
		void op_Trap(TWord op)					{ errNotImplemented(op); }
		void op_Trapcc(TWord op)				{ errNotImplemented(op); }
		void op_Tst(TWord op);
		void op_Vsl(TWord op)					{ errNotImplemented(op); }
		void op_Wait(TWord op);

		// helpers
		void signextend56to64(const JitReg64& _dst, const JitReg64& _src) const;
		void signextend56to64(const JitReg64& _reg) const { return signextend56to64(_reg, _reg); }

		void signextend48to64(const JitReg64& _reg) const;

		void signextend48to56(const JitReg64& _reg) const { signextend48to56(_reg, _reg); }
		void signextend48to56(const JitReg64& _dst, const JitReg64& _src) const;

		void signextend24to56(const JitReg64& _reg) const;

		static void signextend24to64(JitEmitter& _a, const JitReg64& _dst, const JitReg64& _src);
		void signextend24to64(const JitReg64& _dst, const JitReg64& _src) const;
		void signextend24to64(const JitReg64& _reg) const { return signextend24to64(_reg, _reg); }

		void signextend24To32(const JitReg32& _reg) const;

		void bitreverse24(const JitReg32& x) const;

		// The main AGU entry point.
		DspValue updateAddressRegister(TWord _mmm, TWord _rrr, bool _writeR = true, bool _returnPostR = false);

		// used by decode_RRR_read and op_Lua_Rn, and by updateAddressRegister
		void updateAddressRegisterSub(const JitReg32& _r, const JitReg32& _n, uint32_t _rrr, bool _addN);
		void updateAddressRegisterSub(AddressingMode _mode, const JitReg32& _r, const JitReg32& _n, uint32_t _rrr, bool _addN);

		// Parts of the AGU process
		void updateAddressRegisterSubN1(const JitReg32& _r, uint32_t _rrr, bool _addN);
		void updateAddressRegisterSubN1(AddressingMode _mode, const JitReg32& _r, uint32_t _rrr, bool _addN);
		void updateAddressRegisterSubModulo(const JitReg32& _r, const JitReg32& _n, const JitReg32& _m, const JitReg32& _mMask, bool _addN) const;
		void updateAddressRegisterSubModuloN1(const JitReg32& _r, const JitReg32& _m, const JitReg32& _mMask, bool _addN) const;
		void updateAddressRegisterSubMultipleWrapModulo(const JitReg32& _r, const JitReg32& _n, const JitReg32& _mask, bool _addN);
		void updateAddressRegisterSubMultipleWrapModuloN1(const JitReg32& _r, bool _addN, const JitReg32& _mask);
		void updateAddressRegisterSubBitreverse(const JitReg32& _r, const JitReg32& _n, bool _addN);
		void updateAddressRegisterSubBitreverseN1(const JitReg32& _r, bool _addN);

		template<Instruction Inst, ExpectedBitValue BitValue>
		void esaiFrameSyncSpinloop(TWord op) const;

#ifdef HAVE_X86_64
		void signed24To56(const JitReg64& _dst, const JitReg64& _src) const;
#endif
		constexpr static uint64_t signed24To56(const TWord _src)
		{
			return static_cast<uint64_t>((static_cast<int64_t>(_src) << 40ull) >> 8ull) >> 8ull;
		}

		void callDSPFunc(TWord(* _func)(DSP*, TWord)) const;
		void callDSPFunc(void(* _func)(DSP*, TWord)) const;
		void callDSPFunc(void(* _func)(DSP*, TWord), TWord _arg) const;
		void callDSPFunc(void(* _func)(DSP*, TWord), const JitRegGP& _arg) const;

		void setDspProcessingMode(uint32_t _mode);
		
		// Check Condition
		void checkCondition(TWord _cc, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc, bool _updateDirtyCCR, bool _releaseRegPool = true);

		template <Instruction Inst> void checkCondition(const TWord _op, const std::function<void()>& _true, const std::function<void()>& _false, bool _hasFalseFunc, bool _updateDirtyCCR, bool _releaseRegPool = true)
		{
			const TWord cccc = getFieldValue<Inst,Field_CCCC>(_op);

			checkCondition(cccc, _true, _false, _hasFalseFunc, _updateDirtyCCR, _releaseRegPool);
		}

		template <Instruction Inst> void checkCondition(const TWord _op, const std::function<void()>& _true, bool _updateDirtyCCR = true, bool _releaseRegPool = true)
		{
			checkCondition<Inst>(_op, _true, [] {}, false, _updateDirtyCCR, _releaseRegPool);
		}

		// extension word access
		template<Instruction Inst> int pcRelativeAddressExt()
		{
			static_assert(g_opcodes[Inst].m_extensionWordType & PCRelativeAddressExt, "opcode does not have a PC-relative address extension word");
			return signextend<int,24>(getOpWordB());
		}

		template<Instruction Inst> TWord absAddressExt()
		{
			static_assert(g_opcodes[Inst].m_extensionWordType & AbsoluteAddressExt, "opcode does not have an absolute address extension word");
			return getOpWordB();
		}

		// stack
		void	pushPCSR();
		void	popPCSR();
		void	popPC();

		// DSP memory access
		enum EffectiveAddressType
		{
			Immediate,
			Peripherals,
			Memory,
			Dynamic
		};

		template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_MMM, Field_RRR>()>* = nullptr> DspValue effectiveAddress(TWord _op);
		template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_MMM, Field_RRR>()>* = nullptr> EffectiveAddressType effectiveAddressType(TWord _op) const;

		template <Instruction Inst, std::enable_if_t<!hasFieldT<Inst,Field_s>() && has3Fields<Inst, Field_MMM, Field_RRR, Field_S>()>* = nullptr> void readMem(DspValue& _dst, TWord _op);
		template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_MMM, Field_RRR>()>* = nullptr> EffectiveAddressType readMem(DspValue& _dst, TWord _op, EMemArea _area);
		template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>* = nullptr> void readMem(DspValue& _dst, TWord op) const;
		template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>* = nullptr> void readMem(DspValue& _dst, TWord op) const;
		template <Instruction Inst, std::enable_if_t<!hasFieldT<Inst, Field_s>() && hasFields<Inst, Field_aaaaaa, Field_S>()>* = nullptr> void readMem(DspValue& _dst, TWord op) const;
		template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_S, Field_s>() && hasFieldT<Inst, Field_aaaaaa>()>* = nullptr> void readMem(DspValue& _dst, TWord op, EMemArea _area) const;

		template <Instruction Inst, std::enable_if_t<(!hasFields<Inst,Field_s, Field_S>() || Inst==Movep_ppea) && hasFields<Inst, Field_MMM, Field_RRR>()>* = nullptr> EffectiveAddressType writeMem(TWord _op, EMemArea _area, DspValue& _src);
		template <Instruction Inst, std::enable_if_t<!hasFieldT<Inst, Field_s>() && has3Fields<Inst, Field_MMM, Field_RRR, Field_S>()>* = nullptr> void writeMem(TWord _op, DspValue& _src);
		template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>* = nullptr> void writeMem(TWord op, const DspValue& _src);
		template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>* = nullptr> void writeMem(TWord op, const DspValue& _src);
		template <Instruction Inst, std::enable_if_t<!hasAnyField<Inst, Field_S, Field_s>() && hasFieldT<Inst, Field_aaaaaa>()>* = nullptr> void writeMem(TWord op, EMemArea _area, const DspValue& _src) const;
		template<Instruction Inst> void writePmem(TWord _op, const DspValue& _src);

		void readMemOrPeriph(DspValue& _dst, EMemArea _area, const DspValue& _offset, Instruction _inst);
		void writeMemOrPeriph(EMemArea _area, const DspValue& _offset, const DspValue& _value);

		template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_bbbbb, Field_S>()>* = nullptr> void bitTestMemory(TWord _op, ExpectedBitValue _bitValue, asmjit::Label _skip);
		template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_bbbbb>()>* = nullptr> void bitTest(TWord op, DspValue& _value, ExpectedBitValue _bitValue, asmjit::Label _skip) const;

		template <Instruction Inst, std::enable_if_t<hasFields<Inst, Field_bbbbb, Field_S>()>* = nullptr> JitCondCode bitTestMemory(TWord _op, ExpectedBitValue _bitValue);
		template <Instruction Inst, std::enable_if_t<hasFieldT<Inst, Field_bbbbb>()>* = nullptr> JitCondCode bitTest(TWord op, DspValue& _value, ExpectedBitValue _bitValue) const;

		// DSP register access
		void getMR(const JitReg64& _dst) const;
		void getCCR(RegGP& _dst);
		void getCOM(const JitReg64& _dst) const;
		void getEOM(const JitReg64& _dst) const;

		void setMR(const JitReg64& _src) const;
		void setCCR(const JitReg64& _src) const;
		void setCOM(const JitReg64& _src) const;
		void setEOM(const JitReg64& _src) const;

		void getSR(DspValue& _dst);
		void setSR(const DspValue& _src);

		void getXY0(DspValue& _dst, uint32_t _aluIndex, bool _signextend) const;
		void setXY0(uint32_t _xy, const DspValue& _src);
		void getXY1(DspValue& _dst, uint32_t _aluIndex, bool _signextend) const;
		void setXY1(uint32_t _xy, const DspValue& _src);

		void getX0(DspValue& _dst, bool _signextend) const { return getXY0(_dst, 0, _signextend); }
		void getY0(DspValue& _dst, bool _signextend) const { return getXY0(_dst, 1, _signextend); }
		void getX1(DspValue& _dst, bool _signextend) const { return getXY1(_dst, 0, _signextend); }
		void getY1(DspValue& _dst, bool _signextend) const { return getXY1(_dst, 1, _signextend); }

		void getALU0(DspValue& _dst, uint32_t _aluIndex) const;
		void getALU1(DspValue& _dst, uint32_t _aluIndex) const;
		void getALU2signed(DspValue& _dst, uint32_t _aluIndex) const;

		void setALU0(uint32_t _aluIndex, const DspValue& _src) const;
		void setALU1(uint32_t _aluIndex, const DspValue& _src) const;
		void setALU2(uint32_t _aluIndex, const DspValue& _src) const;

		void getSSH(DspValue& _dst) const;
		void setSSH(const DspValue& _src) const;
		void getSSL(DspValue& _dst) const;
		void setSSL(const DspValue& _src) const;
		void setSSHSSL(const DspValue& _ssh, const DspValue& _ssl) const;

		void decSP() const;
		void incSP() const;

		void transferAluTo24(DspValue& _dst, TWord _alu);
		void transfer24ToAlu(TWord _alu, const DspValue& _src) const;
		void transferSaturation24(const JitReg64& _dst, const JitReg64& _src);
		void transferSaturation48(const JitReg64& _dst, const JitReg64& _src);

		// CCR
		class CcrBatchUpdate
		{
		public:
			CcrBatchUpdate(JitOps& _ops, CCRMask _mask);
			CcrBatchUpdate(JitOps& _ops, CCRMask _maskA, CCRMask _maskB);
			CcrBatchUpdate(JitOps& _ops, CCRMask _maskA, CCRMask _maskB, CCRMask _maskC);
			~CcrBatchUpdate();
		private:
			void initialize(CCRMask _mask) const;
			JitOps& m_ops;
		};
		void ccr_update_ifZero(CCRBit _bit);
		void ccr_update_ifNotZero(CCRBit _bit);
		void ccr_update_ifGreater(CCRBit _bit);
		void ccr_update_ifGreaterEqual(CCRBit _bit);
		void ccr_update_ifLess(CCRBit _bit);
		void ccr_update_ifLessEqual(CCRBit _bit);
		void ccr_update_ifCarry(CCRBit _bit);
		void ccr_update_ifNotCarry(CCRBit _bit);
#ifndef HAVE_ARM64
		void ccr_update_ifParity(CCRBit _bit);
		void ccr_update_ifNotParity(CCRBit _bit);
#endif
		void ccr_update_ifAbove(CCRBit _bit);
		void ccr_update_ifBelow(CCRBit _bit);

		void ccr_update(const JitRegGP& _value, CCRBit _bit, bool _valueIsShifted = false);

#ifdef HAVE_ARM64
		void ccr_update(CCRBit _bit, asmjit::arm::CondCode _armConditionCode);
#else
		void ccr_update(CCRBit _bit, asmjit::x86::CondCode _cc);
#endif
		
		void ccr_u_update(const JitReg64& _alu);
		void ccr_e_update(const JitReg64& _alu);
		void ccr_n_update_by55(const JitReg64& _alu);
		void ccr_n_update_by47(const JitReg64& _alu);
		void ccr_n_update_by23(const JitReg64& _alu);
		void ccr_s_update(const JitReg64& _alu);
		void ccr_l_update_by_v();
		void ccr_v_update(const JitReg64& _nonMaskedResult);

		void ccr_clear(CCRMask _mask);
		void ccr_set(CCRMask _mask);
		void ccr_dirty(TWord _aluIndex, const JitReg64& _alu, CCRMask _dirtyBits = static_cast<CCRMask>(CCR_E | CCR_U));
		void ccr_clearDirty(CCRMask _mask);
		void updateDirtyCCR();
		void updateDirtyCCR(CCRMask _whatToUpdate);
		void updateDirtyCCRWithTemp(const JitRegGP& _temp, CCRMask _whatToUpdate);
		void updateDirtyCCR(const JitReg64& _alu, CCRMask _dirtyBits);

		void ccr_getBitValue(const JitRegGP& _dst, CCRBit _bit);
		void sr_getBitValue(const JitRegGP& _dst, SRBit _bit) const;
		void copyBitToCCR(const JitRegGP& _src, uint32_t _bitIndex, CCRBit _dstBit);
		void XYto56(const JitReg64& _dst, int _xy) const;
		void XY0to56(const JitReg64& _dst, int _xy) const;
		void XY1to56(const JitReg64& _dst, int _xy) const;

		// decode
		void decode_cccc(const JitRegGP& _dst, TWord cccc);
#ifdef HAVE_X86_64
		asmjit::x86::CondCode decode_cccc(TWord cccc);
		static asmjit::x86::CondCode reverseCC(asmjit::x86::CondCode cccc);
#else
		asmjit::arm::CondCode decode_cccc(TWord cccc);
		static asmjit::arm::CondCode reverseCC(asmjit::arm::CondCode cccc);
#endif
		void decode_dddddd_read(DspValue& _dst, TWord _dddddd);
		void decode_dddddd_write(TWord _dddddd, const DspValue& _src, bool _sourceIs8Bit = false);
		DspValue decode_dddddd_ref(TWord _dddddd, bool _read = false, bool _write = true) const;
		void decode_ddddd_pcr_read(DspValue& _dst, TWord _ddddd);
		void decode_ddddd_pcr_write(TWord _ddddd, const DspValue& _src);
		void decode_ee_read(DspValue& _dst, TWord _ee);
		void decode_ee_write(TWord _ee, const DspValue& _value);
		DspValue decode_ee_ref(TWord _ee, bool _read, bool _write);
		void decode_EE_read(RegGP& dst, TWord _ee);
		void decode_EE_write(const JitReg64& _src, TWord _ee);
		void decode_ff_read(DspValue& _dst, TWord _ff);
		void decode_ff_write(TWord _ff, const DspValue& _value);
		DspValue decode_ff_ref(TWord _ff, bool _read, bool _write);
		void decode_JJJ_read_56(const JitReg64& _dst, const TWord _jjj, const bool _b) const;
		DspValue decode_JJJ_read_56(TWord _jjj, bool _b) const;
		void decode_JJ_read(DspValue& _dst, TWord jj) const;
		DspValue decode_RRR_read(TWord _rrr, int _shortDisplacement = 0);
		void decode_qq_read(DspValue& _dst, TWord _qq, bool _signextend);
		void decode_QQ_read(DspValue& _dst, TWord _qq, bool _signextend);
		void decode_QQQQ_read(DspValue& _s1, bool _signextendS1, DspValue& _s2, bool _signextendS2, TWord _qqqq) const;
		void decode_qqq_read(DspValue& _dst, TWord _qqq) const;
		void decode_sss_read(DspValue& _dst, TWord _sss) const;
		void decode_LLL_read(TWord _lll, DspValue& x, DspValue& y);
		void decode_LLL_write(TWord _lll, DspValue&& x, DspValue&& y);
		DspValue decode_XMove_MMRRR(TWord _mm, TWord _rrr);

		TWord getOpWordA() const { return m_opWordA; }
		TWord getOpWordB();
		void getOpWordB(DspValue& _dst);

		// ALU
		void unsignedImmediateToAlu(const JitReg64& _r, const uint8_t _i) const;

		void alu_abs(const JitRegGP& _r);
		
		void alu_add(TWord _ab, const JitReg64& _v);
		void alu_add(TWord _ab, uint8_t _v);

		void alu_sub(TWord _ab, const JitReg64& _v);
		void alu_sub(TWord _ab, uint8_t _v);

		void alu_and(TWord ab, DspValue& _v);

		void alu_asl(TWord _abSrc, TWord _abDst, const ShiftReg* _v, TWord _bits = 0);
		void alu_asr(TWord _abSrc, TWord _abDst, const ShiftReg* _v, TWord _immediate = 0);

		void alu_bclr(const DspValue& _dst, TWord _bit);
		void alu_bset(const DspValue& _dst, TWord _bit);
		void alu_bchg(const DspValue& _dst, TWord _bit);

		void alu_cmp(TWord ab, const JitReg64& _v, bool magnitude);

		void alu_lsl(TWord ab, const DspValue& _shiftAmount);
		void alu_lsr(TWord ab, const DspValue& _shiftAmount);
		void alu_eor(TWord ab, DspValue& _v);
		void alu_mpy(TWord ab, DspValue& _s1, DspValue& _s2, bool _negate, bool _accumulate, bool _s1Unsigned, bool _s2Unsigned, bool _round);
		void alu_multiply(TWord op);
		void alu_or(TWord ab, DspValue& _v);
		void alu_rnd(TWord ab);
		void alu_rnd(TWord ab, const JitReg64& d, bool _needsSignextend = true);
		void alu_insert(TWord ab, const DspValue& _src, DspValue& _widthOffset);
		
		template<Instruction Inst> void bitmod_ea(TWord _op, void(JitOps::*_bitmodFunc)(const DspValue&, TWord));
		template<Instruction Inst> void bitmod_aa(TWord _op, void(JitOps::*_bitmodFunc)(const DspValue&, TWord));
		template<Instruction Inst> void bitmod_aa(TWord _op, TWord _addr, void(JitOps::*_bitmodFunc)(const DspValue&, TWord));
		template<Instruction Inst> void bitmod_ppqq(TWord _op, void(JitOps::*_bitmodFunc)(const DspValue&, TWord));
		template<Instruction Inst> void bitmod_D(TWord _op, void(JitOps::*_bitmodFunc)(const DspValue&, TWord));

		// move
		template<Instruction Inst> void move_ddddd_MMMRRR(TWord _op, EMemArea _area);
		template<Instruction Inst> void move_ddddd_absAddr(TWord _op, EMemArea _area);
		template<Instruction Inst> void move_Rnxxxx(TWord op, EMemArea _area);
		template<Instruction Inst> void move_Rnxxx(TWord op, EMemArea _area);
		template<Instruction Inst> void move_L(TWord op);
		template<Instruction Inst> void movep_qqea(TWord op, EMemArea _area);
		template<Instruction Inst> void movep_sqq(TWord op, EMemArea _area);

		void copy24ToDDDDDD(TWord _dddddd, bool _usePooledTemp, const std::function<void(DspValue&)>& _readCallback, bool _readReg = false);
		template<Instruction Inst> void copy24ToDDDDDD(TWord opA, TWord _dddddd, bool _usePooledTemp, const std::function<void(DspValue&)>& _readCallback);

		// loops
		void do_exec(const DspValue& _lc, TWord _addr);
		void do_end(const RegGP& _temp);
		void do_end();
		void rep_exec(TWord _lc);
		void rep_exec(DspValue& _lc);

		// -------------- bra variants
		template<BraMode Bmode> void braOrBsr(DspValue& _offset);

		template<Instruction Inst, BraMode Bmode> void braIfCC(TWord op, DspValue& offset);

		template<Instruction Inst, BraMode Bmode, ExpectedBitValue BitValue> void braIfBitTestMem(TWord op);
		template<Instruction Inst, BraMode Bmode, ExpectedBitValue BitValue> void braIfBitTestDDDDDD(TWord op);

		// -------------- jmp variants
		template<JumpMode Jsr> void jumpOrJSR(DspValue& ea);

		template<Instruction Inst, JumpMode Jsr> void jumpIfCC(TWord op, DspValue& ea);
		
		template<Instruction Inst, JumpMode Jsr, ExpectedBitValue BitValue> void jumpIfBitTestMem(TWord _op);
		template<Instruction Inst, JumpMode Jsr, ExpectedBitValue BitValue> void jumpIfBitTestDDDDDD(TWord op);

		void jmp(DspValue& _absAddr);
		void jsr(DspValue& _absAddr);

		void jmp(TWord _absAddr);
		void jsr(TWord _absAddr);

		TWord getOpSize() const { return m_opSize; }
		Instruction getInstruction() const { return m_instruction; }

		bool checkResultFlag(const ResultFlags _flag) const { return (m_resultFlags & _flag) != 0; }
		uint32_t getResultFlags() const { return m_resultFlags; }

		RegisterMask getWrittenRegs() const { return m_writtenRegs; }
		RegisterMask getReadRegs() const { return m_readRegs; }

		JitBlock& getBlock() const { return m_block; }

		uint32_t getCCRRead() const { return m_ccrRead; }
		uint32_t getCCRWritten() const { return m_ccrWritten; }

		constexpr bool hasAluOp(TWord _op) const { return (_op & 0xff) != 0; }

	private:
		enum RepMode
		{
			RepNone,
			RepFirst,
			RepLoop,
			RepLast
		};

		void errNotImplemented(TWord op);

		JitBlock& m_block;
		JitBlockRuntimeData& m_blockRuntimeData;
		const Opcodes& m_opcodes;
		JitDspRegs& m_dspRegs;
		JitEmitter& m_asm;

		CCRMask& m_ccrDirty;
		bool m_ccr_update_clear = true;

		uint32_t m_ccrRead = CCR_None;
		uint32_t m_ccrWritten = CCR_None;

		TWord m_pcCurrentOp = 0;
		TWord m_opWordA = 0;
		TWord m_opWordB = 0;

		TWord m_opSize = 0;
		Instruction m_instruction = InstructionCount;

		uint32_t m_resultFlags = None;
		RepMode m_repMode = RepNone;
		std::vector<DspValue> m_repTemps;
		RegisterMask m_writtenRegs = RegisterMask::None;
		RegisterMask m_readRegs = RegisterMask::None;
		bool m_fastInterrupt = false;
		bool m_disableCCRUpdates = false;
	};
}
