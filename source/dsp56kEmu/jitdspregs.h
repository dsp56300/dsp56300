#pragma once

#include "jitdspvalue.h"
#include "jitregtracker.h"
#include "jittypes.h"
#include "registers.h"
#include "types.h"

namespace dsp56k
{
	class JitBlock;
	class DSP;

	class JitDspRegs
	{
	public:
		enum AccessType
		{
			Read = 0x1,
			Write = 0x2,
			ReadWrite = Read | Write,
		};

		JitDspRegs(JitBlock& _block);
		~JitDspRegs();

		DspValue getR(TWord _agu) const;

		void getR(DspValue& _dst, TWord _agu) const;
		void getN(DspValue& _dst, TWord _agu) const;
		void getM(DspValue& _dst, TWord _agu) const;

		void setR(TWord _agu, const DspValue& _src) const;
		void setN(TWord _agu, const DspValue& _src) const;
		void setM(TWord _agu, const DspValue& _src) const;

		JitRegGP getSR(AccessType _type) const;
		JitRegGP getLA(AccessType _type) const;
		JitRegGP getLC(AccessType _type) const;

		void getALU(const JitRegGP& _dst, TWord _alu) const;
		DspValue getALU(TWord _alu) const;
		void setALU(TWord _alu, const DspValue& _src, bool _needsMasking = true) const;
		void clrALU(TWord _alu) const;

		void getXY(const JitRegGP& _dst, TWord _xy) const;
		JitRegGP getXY(TWord _xy, AccessType _access) const;
		void setXY(uint32_t _xy, const JitRegGP& _src) const;

		void getEP(DspValue& _dst) const;
		void setEP(const DspValue& _src) const;
		void getVBA(DspValue& _dst) const;
		void setVBA(const DspValue& _src) const;
		void getSC(DspValue& _dst) const;
		void setSC(const DspValue& _src) const;
		void getSZ(DspValue& _dst) const;
		void setSZ(const DspValue& _src) const;
		void getSR(DspValue& _dst) const;
		void setSR(const DspValue& _src) const;
		void getOMR(DspValue& _dst) const;
		void setOMR(const DspValue& _src) const;
		void getSP(const JitReg32& _dst) const;
		void getSP(DspValue& _dst) const;
		void setSP(const DspValue& _src) const;
		void getLA(DspValue& _dst) const;
		void setLA(const DspValue& _src) const;
		void getLC(DspValue& _dst) const;
		void setLC(const DspValue& _src) const;

		void getSS(const JitReg64& _dst) const;
		void setSS(const JitReg64& _src) const;
		void modifySS(const std::function<void(const JitReg64&)>& _func, bool _read, bool _write) const;

		void mask56(const JitRegGP& _alu) const;
		void mask48(const JitRegGP& _alu) const;
		
		void setPC(const DspValue& _pc) const;
		CCRMask& ccrDirtyFlags() { return m_ccrDirtyFlags; }

		void reset();

	private:
		JitDspRegPool& pool() const;

		void load24(DspValue& _dst, const TReg24& _src) const;
		void load24(const JitRegGP& _dst, const TReg24& _src) const;
		void store24(TReg24& _dst, const JitRegGP& _src) const;
		void store24(TReg24& _dst, const DspValue& _src) const;

		JitBlock& m_block;
		JitEmitter& m_asm;
		DSP& m_dsp;

		CCRMask m_ccrDirtyFlags = static_cast<CCRMask>(0);
	};
}
