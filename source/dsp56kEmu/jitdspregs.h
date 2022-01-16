#pragma once

#include <array>

#include "jitregtracker.h"
#include "jitregtypes.h"
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

		void getR(const JitRegGP& _dst, int _agu);
		void getN(const JitRegGP& _dst, int _agu);
		void getM(const JitRegGP& _dst, int _agu);

		void setR(int _agu, const JitRegGP& _src);
		void setN(int _agu, const JitRegGP& _src);
		void setM(int _agu, const JitRegGP& _src);
		
		JitRegGP getSR(AccessType _type);
		JitRegGP getLA(AccessType _type);
		JitRegGP getLC(AccessType _type);

		JitRegGP getALU(int _alu, AccessType _access);
		void getALU(const JitRegGP& _dst, int _alu);
		void setALU(int _alu, const JitRegGP& _src, bool _needsMasking = true);
		void clrALU(TWord _alu);

		void getXY(const JitRegGP& _dst, int _xy);
		JitRegGP getXY(int _xy, AccessType _access);
		void setXY(uint32_t _xy, const JitRegGP& _src);

		void getEP(const JitReg32& _dst) const;
		void setEP(const JitReg32& _src) const;
		void getVBA(const JitReg32& _dst) const;
		void setVBA(const JitReg32& _src) const;
		void getSC(const JitReg32& _dst) const;
		void setSC(const JitReg32& _src) const;
		void getSZ(const JitReg32& _dst) const;
		void setSZ(const JitReg32& _src) const;
		void getSR(const JitReg32& _dst);
		void setSR(const JitReg32& _src);
		void getOMR(const JitReg32& _dst) const;
		void setOMR(const JitReg32& _src) const;
		void getSP(const JitReg32& _dst) const;
		void setSP(const JitReg32& _src) const;
		void getLA(const JitReg32& _dst);
		void setLA(const JitReg32& _src);
		void getLC(const JitReg32& _dst);
		void setLC(const JitReg32& _src);

		void getSS(const JitReg64& _dst) const;
		void setSS(const JitReg64& _src) const;
		void modifySS(const std::function<void(const JitReg64&)>& _func, bool _read, bool _write) const;

		void mask56(const JitRegGP& _alu) const;
		void mask48(const JitRegGP& _alu) const;
		
		void setPC(const JitRegGP& _pc);
		CCRMask& ccrDirtyFlags() { return m_ccrDirtyFlags; }

	private:
		JitDspRegPool& pool() const;

		void load24(const JitRegGP& _dst, const TReg24& _src) const;
		void store24(TReg24& _dst, const JitRegGP& _src) const;

		JitBlock& m_block;
		JitEmitter& m_asm;
		DSP& m_dsp;

		CCRMask m_ccrDirtyFlags = static_cast<CCRMask>(0);
	};
}
