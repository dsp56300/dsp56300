#pragma once

#include "jitdspregpool.h"
#include "jitregtracker.h"

namespace dsp56k
{
	class JitBlock;
	class DSPReg;

	class DspValue
	{
	public:
		enum Type
		{
			Invalid = -1,

			Memory,

			DspReg56,
			DspReg48,
			DspReg24,
			DspReg8,

			Immediate56,
			Immediate48,
			Immediate24,
			Immediate8,

			Temp56,
			Temp48,
			Temp24,
			Temp8
		};

		explicit DspValue(JitBlock& _block, bool _usePooledTemp = false, bool _useScratchTemp = false);
		explicit DspValue(JitBlock& _block, int64_t _value, Type _type = Immediate56);
		explicit DspValue(JitBlock& _block, int _value, Type _type = Immediate24);
		explicit DspValue(JitBlock& _block, TWord _value, Type _type = Immediate24) : DspValue(_block, static_cast<int>(_value), _type) {}
		explicit DspValue(JitBlock& _block, PoolReg _reg, bool _read, bool _write);

		explicit DspValue(JitBlock& _block, PoolReg _reg, bool _read, bool _write, const TWord _regOffset)
			: DspValue(_block, static_cast<PoolReg>(_reg + _regOffset), _read, _write)
		{
		}

		DspValue(const DspValue&) = delete;

		DspValue(DspValue&& _other) noexcept;

		explicit DspValue(DSPReg&& _dspReg);
		explicit DspValue(DSPRegTemp&& _dspReg, Type _type);
		explicit DspValue(RegGP&& _existingTemp, Type _type);
		explicit DspValue(RegScratch&& _existingTemp, Type _type);

		void set(const JitRegGP& _reg, Type _type);
		void set(const int32_t& _value, Type _type);
		void set(const TWord& _value, Type _type)
		{
			set(static_cast<int32_t>(_value), _type);
		}
		void set(const int64_t& _value, Type _type);
		void temp(Type _type);

		JitRegGP& get()
		{
			if (!isRegValid())
				immToTemp();
			return m_reg;
		}

		const JitRegGP& get() const { assert(isRegValid()); return m_reg; }

		bool isRegValid() const
		{
			return m_reg.isValid();
		}

		Type type() const { return m_type; }

		bool isType(const Type _type) const
		{
			return m_type == _type;
		}

		TWord imm24() const
		{
			assert(isType(Immediate24));
			return static_cast<TWord>(m_immediate);
		}

		int64_t& imm()
		{
			return m_immediate;
		}

		const int64_t& imm() const
		{
			return m_immediate;
		}

		bool isImm24() const
		{
			return isType(Immediate24);
		}

		void immToTemp();
		void immToReg(const JitRegGP& _reg) const;
		void immToReg(Type _type, const JitRegGP& _reg) const;

		TWord getBitCount() const { return m_bitSize; }
		bool isImmediate() const;
		void copyTo(const JitRegGP& _dst, TWord _dstBitCount) const;
		void copyTo(const DspValue& _dst) const;

		bool isDspReg(PoolReg _reg) const;
		void write();
		void convertTo56(const JitReg64& _dst) const;
		void convertTo56(const JitRegGP& _dst, const JitRegGP& _src) const;

		void convertSigned8To24();
		void convertUnsigned8To24();
		void toTemp();

		bool isTemp() const;
		void release();

		static TWord getBitCount(PoolReg _reg);
		static TWord getBitCount(Type _reg);

		static Type getDspRegTypeFromBitSize(TWord _bitSize);

		DspValue& operator = (DspValue&& _other) noexcept;

		static Type immTypeToTempType(Type _immType);

		const DSPReg& getDspReg() const { return m_dspReg; }

		void setUsePooledTemp(bool _pooled);
		void setUseScratchTemp(bool _scratch);

		void reinterpretAs(const Type _type, const TWord _bitCount)
		{
			m_type = _type;
			m_bitSize = _bitCount;
		}

	private:
		JitReg64 temp() const
		{
			return m_usePooledTemp ? m_pooledTemp.get() : (m_useScratchTemp ? m_scratch.get() : m_gpTemp.get());
		}

		void acquireTemp();
		void releaseTemp();

		bool isTempAcquired() const
		{
			if(m_usePooledTemp)
				return m_pooledTemp.acquired();
			if(m_useScratchTemp)
				return m_scratch.isValid();
			return m_gpTemp.isValid();
		}

		JitBlock& m_block;

		bool m_usePooledTemp = false;
		bool m_useScratchTemp = false;

		RegGP m_gpTemp;
		DSPRegTemp m_pooledTemp;
		RegScratch m_scratch;
		DSPReg m_dspReg;
		JitRegGP m_reg;

		TWord m_bitSize;
		Type m_type = Invalid;
		int64_t m_immediate = 0;
	};

	static DspValue makeDspValueAguReg(JitBlock& _block, const PoolReg _aguBaseReg, const TWord _aguRegIndex, const bool _read = true, const bool _write = false)
	{
		return DspValue(_block, static_cast<PoolReg>(_aguBaseReg + _aguRegIndex), _read, _write);
	}

	static DspValue makeDspValueRegR(JitBlock& _block, const TWord _aguRegRindex, const bool _read = true, const bool _write = false)
	{
		return makeDspValueAguReg(_block, PoolReg::DspR0, _aguRegRindex, _read, _write);
	}
}

#define UsePooledTemp true
