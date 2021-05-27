#pragma once

#include "jittypes.h"
#include "types.h"

namespace asmjit
{
	namespace x86
	{
		class Assembler;
	}
}

namespace dsp56k
{
	class JitBlock;
	class DSP;

	class JitDspRegs
	{
	public:
		JitDspRegs(JitBlock& _block);

		~JitDspRegs();

		void clear() { storeDSPRegs(); assert(m_loadedRegs == 0); }

		void getR(const JitReg& _dst, int _agu);
		void getN(const JitReg& _dst, int _agu);
		void getM(const JitReg& _dst, int _agu);

		void setR(int _agu, const JitReg& _src);
		void setN(int _agu, const JitReg& _src);
		void setM(int _agu, const JitReg& _src);

		JitReg getSR();
		JitReg getExtMemAddr();
		JitReg getLA();
		JitReg getLC();

		void getALU(const JitReg& _dst, int _alu);
		void setALU(int _alu, const JitReg& _src);

		void getXY(const JitReg& _dst, int _xy);
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

		void notifyBeginBranch()
		{
			/*
			If code is generated which uses the DspRegs state machine and this is NOT executed because of a branch,
			the state machine will emit register restore in its destructor but the load of these registers is missing.
			The only workaround currently available is to emit loading code for all registers. Can be improved later
			by only emitting loading code for register that have actually been loaded in the code that has not been
			executed.
			*/

			loadDSPRegs();
		}
	private:
		enum LoadedRegs
		{
			LoadedRegR0,	LoadedRegR1,	LoadedRegR2,	LoadedRegR3,	LoadedRegR4,	LoadedRegR5,	LoadedRegR6,	LoadedRegR7,
			LoadedRegA,		LoadedRegB,
			LoadedRegX,		LoadedRegY,
			LoadedRegExtMem,
			LoadedRegSR,
			LoadedRegLC,
			LoadedRegLA
		};

		void loadDSPRegs();
		void storeDSPRegs();

		void loadAGU(int _agu);
		void loadALU(int _alu);
		void loadXY(int _xy);

		void storeAGU(int _agu);
		void storeALU(int _alu);
		void storeXY(int _xy);

		void load24(const asmjit::x86::Gp& _dst, TReg24& _src) const;
		void store24(TReg24& _dst, const asmjit::x86::Gp& _src) const;

		bool isLoaded(uint32_t _reg) const;
		void setLoaded(const uint32_t _reg)				{ m_loadedRegs |= (1<<_reg); }
		void setUnloaded(const uint32_t _reg)			{ m_loadedRegs &= ~(1<<_reg); }
		uint32_t getLoadedRegs() const					{ return m_loadedRegs; }

		JitBlock& m_block;
		asmjit::x86::Assembler& m_asm;
		DSP& m_dsp;

		uint32_t m_loadedRegs = 0;
	};
}
