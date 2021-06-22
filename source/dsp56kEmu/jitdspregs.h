#pragma once

#include <array>

#include "jitregtracker.h"
#include "jitregtypes.h"
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
		enum LoadedRegs
		{
			LoadedRegR0,	LoadedRegR1,	LoadedRegR2,	LoadedRegR3,	LoadedRegR4,	LoadedRegR5,	LoadedRegR6,	LoadedRegR7,
			LoadedRegA,		LoadedRegB,
			LoadedRegX,		LoadedRegY,
			LoadedRegExtMem,
			LoadedRegSR,
			LoadedRegLC,
			LoadedRegLA,
			LoadedRegCount
		};

		enum AccessType
		{
			Read = 0x1,
			Write = 0x2,
			ReadWrite = Read | Write,
		};

		JitDspRegs(JitBlock& _block);

		~JitDspRegs();

		void clear() { storeDSPRegs(); assert(m_writtenRegs == 0); }

		void getR(const JitReg& _dst, int _agu);
		void getN(const JitReg& _dst, int _agu);
		void getM(const JitReg& _dst, int _agu);

		void setR(int _agu, const JitReg& _src);
		void setN(int _agu, const JitReg& _src);
		void setM(int _agu, const JitReg& _src);
		
		void getParallel0(const JitReg& _dst);
		void getParallel1(const JitReg& _dst);
		void setParallel0(const JitReg& _dst);
		void setParallel1(const JitReg& _dst);


		JitReg getSR(AccessType _type);
		JitReg getExtMemAddr();
		JitReg getLA(AccessType _type);
		JitReg getLC(AccessType _type);

		JitReg128 getALU(int _ab);
		void getALU(const JitReg& _dst, int _alu);
		void setALU(int _alu, const JitReg& _src, bool _needsMasking = true);
		void clrALU(const TWord _aluIndex);

		JitReg128 getXY(int _xy);
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

		void storeDSPRegs(uint32_t _loadedRegs);
		
		void setPC(const JitReg& _pc);
		void updateDspMRegisters();

		uint32_t getWrittenRegs() const { return m_writtenRegs; }
		uint32_t getReadRegs() const { return m_readRegs; }

		bool isRead(uint32_t _reg) const;
		bool isWritten(uint32_t _reg) const;

	private:
		void loadDSPRegs();
		void storeDSPRegs();

		void loadAGU(int _agu);
		void loadALU(int _alu);
		void loadXY(int _xy);

		void storeAGU(int _agu);
		void storeALU(int _alu);
		void storeXY(int _xy);

		void load24(const asmjit::x86::Gp& _dst, const TReg24& _src) const;
		void store24(TReg24& _dst, const asmjit::x86::Gp& _src) const;

		void setWritten(const uint32_t _reg)			{ m_writtenRegs |= (1<<_reg); }
		void clearWritten(const uint32_t _reg)			{ m_writtenRegs &= ~(1<<_reg); }

		void setRead(const uint32_t _reg)				{ m_readRegs |= (1<<_reg); }
		void clearRead(const uint32_t _reg)				{ m_readRegs &= ~(1<<_reg); }

		void load(LoadedRegs _reg);
		void store(LoadedRegs _reg);
		
		JitBlock& m_block;
		asmjit::x86::Assembler& m_asm;
		DSP& m_dsp;

		uint32_t m_writtenRegs = 0;
		uint32_t m_readRegs = 0;
		std::array<uint32_t, 8> m_AguMchanged;
	};

	class JitDspRegsBranch
	{
	public:
		JitDspRegsBranch(JitDspRegs& _regs);
		~JitDspRegsBranch();

		JitDspRegs& m_regs;
		const uint32_t m_loadedRegsBeforeBranch;
	};
}
