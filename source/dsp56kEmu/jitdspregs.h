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

		void getR(const JitReg& _dst, int _agu);
		void getN(const JitReg& _dst, int _agu);
		void getM(const JitReg& _dst, int _agu);

		void setR(int _agu, const JitReg& _src);
		void setN(int _agu, const JitReg& _src);
		void setM(int _agu, const JitReg& _src);
		
		JitReg getSR(AccessType _type);
		JitReg getLA(AccessType _type);
		JitReg getLC(AccessType _type);

		JitReg getALU(int _alu, AccessType _access);
		void getALU(const JitReg& _dst, int _alu);
		void setALU(int _alu, const JitReg& _src, bool _needsMasking = true);
		void clrALU(TWord _alu);

		void getXY(const JitReg& _dst, int _xy);
		JitReg getXY(int _xy, AccessType _access);
		void setXY(uint32_t _xy, const JitReg& _src);

		void getXY0(const JitReg& _dst, uint32_t _aluIndex);
		void setXY0(uint32_t _xy, const JitReg& _src);
		void getXY1(const JitReg& _dst, uint32_t _aluIndex);
		void setXY1(uint32_t _xy, const JitReg& _src);

		void getX0(const JitReg& _dst) { return getXY0(_dst, 0); }
		void getY0(const JitReg& _dst) { return getXY0(_dst, 1); }
		void getX1(const JitReg& _dst) { return getXY1(_dst, 0); }
		void getY1(const JitReg& _dst) { return getXY1(_dst, 1); }

		void getALU0(const JitReg& _dst, uint32_t _aluIndex);
		void setALU0(uint32_t _aluIndex, const JitReg& _src);
		void getALU1(const JitReg& _dst, uint32_t _aluIndex);
		void setALU1(uint32_t _aluIndex, const JitReg32& _src);
		void getALU2signed(const JitReg& _dst, uint32_t _aluIndex);
		void setALU2(uint32_t _aluIndex, const JitReg32& _src);

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
		void getSSH(const JitReg32& _dst) const;
		void setSSH(const JitReg32& _src) const;
		void getSSL(const JitReg32& _dst) const;
		void setSSL(const JitReg32& _src) const;
		void getLA(const JitReg32& _dst);
		void setLA(const JitReg32& _src);
		void getLC(const JitReg32& _dst);
		void setLC(const JitReg32& _src);

		void getSS(const JitReg64& _dst) const;
		void setSS(const JitReg64& _src) const;

		void decSP() const;
		void incSP() const;

		void mask56(const JitReg& _alu) const;
		void mask48(const JitReg& _alu) const;
		
		void setPC(const JitReg& _pc);
		CCRMask& ccrDirtyFlags() { return m_ccrDirtyFlags; }

	private:
		JitDspRegPool& pool() const;

		void load24(const JitReg& _dst, const TReg24& _src) const;
		void store24(TReg24& _dst, const JitReg& _src) const;

		JitBlock& m_block;
		JitAssembler& m_asm;
		DSP& m_dsp;

		CCRMask m_ccrDirtyFlags = static_cast<CCRMask>(0);
	};
}
