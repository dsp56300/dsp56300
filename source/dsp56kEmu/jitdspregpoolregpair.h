#pragma once

#include <array>

#include "jitdspregpooltypes.h"
#include "jittypes.h"
#include "types.h"

namespace dsp56k
{
	class JitEmitter;
	class JitBlock;
	class DspValue;
	class JitDspRegPool;

	class JitRegPoolRegPair
	{
	public:
		JitRegPoolRegPair(JitDspRegPool& _pool, PoolReg _base, PoolReg _low, PoolReg _high) : m_pool(_pool), m_regBase(_base), m_reg0(_low), m_reg1(_high)
		{
		}

		~JitRegPoolRegPair() = default;

		JitRegPoolRegPair(const JitRegPoolRegPair&) = delete;
		JitRegPoolRegPair(JitRegPoolRegPair&&) = delete;
		JitRegPoolRegPair& operator = (const JitRegPoolRegPair&) = delete;
		JitRegPoolRegPair& operator = (JitRegPoolRegPair&&) = delete;

		void get(const JitReg64& _dst) const;

		void get0(const JitReg32& _dst) const;
		void get1(const JitReg32& _dst) const;

		DspValue get0(bool _read, bool _write) const;
		DspValue get1(bool _read, bool _write) const;

		void set0(const DspValue& _src) const;
		void set1(const DspValue& _src) const;

		void store0(const JitReg32& _src) const;
		void store1(const JitReg32& _src) const;

		void load0(const JitReg32& _dst) const;
		void load1(const JitReg32& _dst) const;

	private:
		void store01(const JitReg32& _reg0, const JitReg32& _reg1) const;

		void merge(const JitReg64& _dst, const JitReg32& _reg1, const JitReg32& _reg0) const;
		void replaceLow(const JitReg64& _dst, const JitReg32& _src) const;
		void replaceHigh(const JitReg64& _temp, const JitReg64& _dst, const JitReg32& _src) const;
		void extractLow(const JitReg32& _dst, const JitRegGP& _src) const;
		void extractHigh(const JitReg32& _dst, const JitRegGP& _src) const;

		void mov(const JitReg32& _dst, const JitRegGP& _src) const;
		void mov(const JitReg32& _dst, PoolReg _src) const;

		const TReg48& dspReg() const;

		JitBlock& getBlock() const;
		JitEmitter& asm_() const;
		JitDspRegPool& m_pool;

		const PoolReg m_regBase;
		const PoolReg m_reg0;
		const PoolReg m_reg1;

		JitReg64 m_reg;
	};
}
