#include "jitdspvalue.h"

#include "jitblock.h"
#include "jitemitter.h"
#include "jithelper.h"

namespace dsp56k
{
	DspValue::DspValue(JitBlock& _block, bool _usePooledTemp/* = false*/, bool _useScratchTemp/* = false*/, bool _useShiftTemp/* = false*/)
		: m_block(_block)
		, m_usePooledTemp(_usePooledTemp)
		, m_useScratchTemp(_useScratchTemp)
		, m_useShiftTemp(_useShiftTemp)
		, m_gpTemp(_block, false)
		, m_pooledTemp(_block, false)
		, m_scratch(_block, false)
		, m_shift(_block, false)
		, m_dspReg(_block, PoolReg::DspRegInvalid, false, false, false)
		, m_bitSize(0)
	{
	}

	DspValue::DspValue(JitBlock& _block, const int _value, const Type _type) : DspValue(_block)
	{
		set(_value, _type);
	}

	DspValue::DspValue(JitBlock& _block, const int64_t _value, const Type _type) : DspValue(_block)
	{
		set(_value, _type);
	}

	DspValue::DspValue(JitBlock& _block, PoolReg _reg, bool _read, bool _write)
		: m_block(_block)
		, m_gpTemp(_block, false)
		, m_pooledTemp(_block, false)
		, m_scratch(_block, false)
		, m_shift(_block, false)
		, m_dspReg(_block, _reg, _read, _write)
		, m_reg(getBitCount(_reg) <= 32 ? static_cast<JitRegGP>(m_dspReg.r32()) : static_cast<JitRegGP>(m_dspReg.r64()))
		, m_bitSize(getBitCount(_reg))
		, m_type(getDspRegTypeFromBitSize(m_bitSize))
	{
	}

	DspValue::DspValue(DSPReg&& _dspReg)
		: m_block(_dspReg.block())
		, m_gpTemp(_dspReg.block(), false)
		, m_pooledTemp(_dspReg.block(), false)
		, m_scratch(_dspReg.block(), false)
		, m_shift(_dspReg.block(), false)
		, m_dspReg(std::move(_dspReg))
		, m_reg(getBitCount(m_dspReg.dspReg()) <= 32 ? static_cast<JitRegGP>(m_dspReg.r32()) : static_cast<JitRegGP>(m_dspReg.r64()))
		, m_bitSize(getBitCount(m_dspReg.dspReg()))
		, m_type(getDspRegTypeFromBitSize(m_bitSize))
	{
	}

	DspValue::DspValue(DSPRegTemp&& _dspReg, const Type _type)
		: m_block(_dspReg.block())
		, m_usePooledTemp(true)
		, m_useScratchTemp(false)
		, m_useShiftTemp(false)
		, m_gpTemp(_dspReg.block(), false)
		, m_pooledTemp(std::move(_dspReg))
		, m_scratch(_dspReg.block(), false)
		, m_shift(_dspReg.block(), false)
		, m_dspReg(_dspReg.block(), PoolReg::DspRegInvalid, false, false, false)
		, m_reg(getBitCount(_type) <= 32 ? static_cast<JitRegGP>(r32(temp())) : static_cast<JitRegGP>(r64(temp())))
		, m_bitSize(getBitCount(_type))
		, m_type(_type)
	{
	}

	DspValue::DspValue(RegGP&& _existingTemp, const Type _type)
		: m_block(_existingTemp.block())
		, m_usePooledTemp(false)
		, m_useScratchTemp(false)
		, m_useShiftTemp(false)
		, m_gpTemp(std::move(_existingTemp))
		, m_pooledTemp(_existingTemp.block(), false)
		, m_scratch(_existingTemp.block(), false)
		, m_shift(_existingTemp.block(), false)
		, m_dspReg(_existingTemp.block(), PoolReg::DspRegInvalid, false, false, false)
		, m_reg(getBitCount(_type) <= 32 ? static_cast<JitRegGP>(r32(temp())) : static_cast<JitRegGP>(r64(temp())))
		, m_bitSize(getBitCount(_type))
		, m_type(_type)
	{
	}

	DspValue::DspValue(RegScratch&& _existingTemp, const Type _type)
		: m_block(_existingTemp.block())
		, m_usePooledTemp(false)
		, m_useScratchTemp(true)
		, m_useShiftTemp(false)
		, m_gpTemp(_existingTemp.block(), false)
		, m_pooledTemp(_existingTemp.block(), false)
		, m_scratch(std::move(_existingTemp))
		, m_shift(_existingTemp.block(), false)
		, m_dspReg(_existingTemp.block(), PoolReg::DspRegInvalid, false, false, false)
		, m_reg(getBitCount(_type) <= 32 ? static_cast<JitRegGP>(r32(temp())) : static_cast<JitRegGP>(r64(temp())))
		, m_bitSize(getBitCount(_type))
		, m_type(_type)
	{
	}

	DspValue::DspValue(DspValue&& _other) noexcept
		: m_block(_other.m_block)
		, m_gpTemp(_other.m_block, false)
		, m_pooledTemp(_other.m_block, false)
		, m_scratch(_other.m_block, false)
		, m_shift(_other.m_block, false)
		, m_dspReg(_other.m_block, PoolReg::DspRegInvalid, false, false, false)
		, m_bitSize(0)
	{
		*this = std::move(_other);
	}

	void DspValue::set(const JitRegGP& _reg, const Type _type)
	{
		m_reg = r64(_reg);
		m_type = _type;
		m_bitSize = getBitCount(_type);
	}

	void DspValue::set(const int64_t& _value, const Type _type)
	{
		release();
		m_immediate = _value;
		m_type = _type;
		m_bitSize = getBitCount(_type);
	}

	void DspValue::set(const int32_t& _value, const Type _type)
	{
		release();
		m_immediate = _value;
		m_type = _type;
		m_bitSize = getBitCount(_type);
	}

	void DspValue::temp(const Type _type)
	{
		assert(!m_dspReg.acquired());	// TODO: requires copy
		if (m_dspReg.acquired())
			m_dspReg.release();

		if(!isTempAcquired())
			acquireTemp();

		m_bitSize = getBitCount(_type);

		m_type = _type;

		if(m_bitSize <= 32)
		{
			m_reg = r32(temp());
		}
		else if(_type == Temp56)
		{
			m_reg = r64(temp());
		}
		else
		{
			assert(false && "invalid DSP value type");
		}
	}

	void DspValue::immToTemp()
	{
		if (m_reg.isValid())
			return;

		const auto immType = m_type;
		const auto tempType = immTypeToTempType(immType);

		temp(tempType);
		immToReg(immType, m_reg);
	}

	void DspValue::immToReg(const JitRegGP& _reg) const
	{
		immToReg(m_type, _reg);
	}

	void DspValue::immToReg(const Type _type, const JitRegGP& _reg) const
	{
		const auto imm = asmjit::Imm(m_immediate);

		switch (_type)
		{
		case Immediate8:
		case Immediate24:
			m_block.asm_().mov(r32(_reg), imm);
			break;
		case Immediate48:
		case Immediate56:
			m_block.asm_().mov(r64(_reg), imm);
			break;
		default:
			assert(false && "unknown DSP value type");
			break;
		}
	}

	bool DspValue::isImmediate() const
	{
		switch (m_type)
		{
		case Immediate56:
		case Immediate48:
		case Immediate24:
		case Immediate8:
			return true;
		default:
			return false;
		}
	}

	void DspValue::copyTo(const JitRegGP& _dst, const TWord _dstBitCount) const
	{
		assert(_dstBitCount == getBitCount() && "copy requires type conversion");

		if (isImmediate())
		{
			if(m_immediate == 0)
				m_block.asm_().clr(_dst);
			else if(getBitCount() <= 32)
				m_block.asm_().mov(r32(_dst), asmjit::Imm(static_cast<TWord>(m_immediate)));
			else
				m_block.asm_().mov(r64(_dst), asmjit::Imm(m_immediate));
		}
		else
		{
			if (r32(_dst) != r32(get()))
			{
				if (getBitCount() <= 32)
					m_block.asm_().mov(r32(_dst), r32(get()));
				else
					m_block.asm_().mov(r64(_dst), r64(get()));
			}
			else
			{
				assert(false);
			}
		}
	}

	void DspValue::copyTo(const DspValue& _dst) const
	{
		copyTo(_dst.get(), _dst.getBitCount());
	}

	bool DspValue::isDspReg(const PoolReg _reg) const
	{
		if (m_dspReg.dspReg() != _reg)
			return false;
		if (!m_dspReg.acquired())
			return false;
		assert(m_reg.isValid());
		return true;
	}

	void DspValue::write()
	{
		assert(m_dspReg.acquired());
		m_dspReg.write();
	}

	void DspValue::convertTo56(const JitReg64& _dst) const
	{
		if (getBitCount() == 56)
		{
			if(isImmediate())
			{
				m_block.asm_().mov(_dst, asmjit::Imm(m_immediate));
			}
			else
			{
				m_block.asm_().mov(_dst, r64(m_reg));
			}
			return;
		}

		assert(getBitCount() == 24);

		if (isImmediate())
		{
			auto u = static_cast<uint64_t>(m_immediate);
			auto& s = reinterpret_cast<int64_t&>(u);
			u <<= 40ull;
			s >>= 8ll;
			u >>= 8ull;

			m_block.asm_().mov(_dst, asmjit::Imm(u));
		}
		else
		{
			assert(m_type == DspReg24 || m_type == Temp24 || m_type == Memory);

			convertTo56(_dst, m_reg);
		}
	}

	void DspValue::convertTo56(const JitRegGP& _dst, const JitRegGP& _src) const
	{
#ifdef HAVE_ARM64
		m_block.asm_().sbfx(r32(_dst), r32(_src), asmjit::Imm(0), asmjit::Imm(24));
		m_block.asm_().lsl(r64(_dst), r64(_dst), asmjit::Imm(24));
#else
		if(r32(_dst) != r32(_src))
		{
			m_block.asm_().rol(r64(_dst), r32(_src), 40);
		}
		else
		{
			m_block.asm_().shl(r64(_dst), asmjit::Imm(40));
		}
		m_block.asm_().sar(r64(_dst), asmjit::Imm(8));
		m_block.asm_().shr(r64(_dst), asmjit::Imm(8));
#endif
	}

	void DspValue::convertSigned8To24()
	{
		if (getBitCount() == 24)
			return;

		assert(getBitCount() == 8);

		if (isImmediate())
		{
			m_immediate <<= 16;
			m_type = Immediate24;
		}
		else
		{
			assert(m_type == Temp8);
			m_block.asm_().shl(m_reg, asmjit::Imm(16));
			m_type = Temp24;
		}

		m_bitSize = 24;
	}

	void DspValue::convertUnsigned8To24()
	{
		if (getBitCount() == 24)
			return;

		assert(getBitCount() == 8);

		if (isImmediate())
		{
			m_type = Immediate24;
		}
		else
		{
			assert(m_type == Temp8);
			m_type = Temp24;
		}

		m_bitSize = 24;
	}

	void DspValue::toTemp()
	{
		if (isTemp())
			return;

		acquireTemp();

		if (getBitCount() <= 32)
			m_reg = r32(temp());
		else
			m_reg = r64(temp());

		if(isImmediate())
		{
			m_block.asm_().mov(m_reg, asmjit::Imm(m_immediate));
		}
		else
		{
			assert(m_dspReg.acquired());
			if (getBitCount() <= 32)
				m_block.asm_().mov(m_reg, r32(m_dspReg.get()));
			else
				m_block.asm_().mov(m_reg, r64(m_dspReg.get()));
			m_dspReg.release();
		}

		switch (getBitCount())
		{
		case 8:		m_type = Temp8;								break;
		case 24:	m_type = Temp24;							break;
		case 48:	m_type = Temp48;							break;
		case 56:	m_type = Temp56;							break;
		default:	assert(false && "unknown bit count");	break;
		}
	}

	bool DspValue::isTemp() const
	{
		switch (m_type)
		{
		case Temp8:
		case Temp24:
		case Temp48:
		case Temp56:
			assert(isRegValid());
			return true;
		default:
			return false;
		}
	}

	void DspValue::release()
	{
		if (m_dspReg.acquired())
			m_dspReg.release();

		releaseTemp();
		m_reg.reset();
	}

	TWord DspValue::getBitCount(const PoolReg _reg)
	{
		switch (_reg)
		{
		case PoolReg::DspA:
		case PoolReg::DspB:
		case PoolReg::DspAwrite:
		case PoolReg::DspBwrite:
			return 56;
		case PoolReg::DspX:
		case PoolReg::DspY:
			return 48;
		default:
			return 24;
		}
	}

	TWord DspValue::getBitCount(const Type _reg)
	{
		switch (_reg)
		{
		case Temp56:
		case DspReg56:
		case Immediate56:		return 56;

		case Temp48:
		case DspReg48:
		case Immediate48:		return 48;

		case Memory:
		case Temp24:
		case DspReg24:
		case Immediate24:		return 24;

		case Temp8:
		case DspReg8:
		case Immediate8:		return 8;

		default:
			assert(false && "invalid dsp value type");
			return 0;
		}
	}

	DspValue::Type DspValue::getDspRegTypeFromBitSize(const TWord _bitSize)
	{
		switch (_bitSize)
		{
		case 56:
			return DspReg56;
		case 48:
			return DspReg48;
		case 24:
			return DspReg24;
		default:
			assert(false && "invalid bit size");
			return Invalid;
		}
	}

	DspValue& DspValue::operator=(DspValue&& _other) noexcept
	{
		m_gpTemp = std::move(_other.m_gpTemp);
		m_pooledTemp = std::move(_other.m_pooledTemp);
		m_scratch = std::move(_other.m_scratch);
		m_shift = std::move(_other.m_shift);
		m_dspReg = std::move(_other.m_dspReg);

		m_reg = _other.m_reg;
		m_bitSize = _other.m_bitSize;
		m_type = _other.m_type;
		m_immediate = _other.m_immediate;
		m_usePooledTemp = _other.m_usePooledTemp;
		m_useScratchTemp = _other.m_useScratchTemp;
		m_useShiftTemp = _other.m_useShiftTemp;

		_other.m_reg.reset();
		_other.m_bitSize = 0;
		_other.m_immediate = static_cast<int64_t>(0xde1de1de1de1de1d);

		return *this;
	}

	DspValue::Type DspValue::immTypeToTempType(const Type _immType)
	{
		switch (_immType)
		{
		case Immediate8:
			return Temp8;
		case Immediate24:
			return Temp24;
		case Immediate48:
			return Temp48;
		case Immediate56:
			return Temp56;
		default:
			assert(false && "unknown DSP value type");
			return Invalid;
		}
	}

	void DspValue::setUsePooledTemp(bool _pooled)
	{
		assert(!isTempAcquired());
		m_usePooledTemp = _pooled;
	}

	void DspValue::setUseScratchTemp(bool _scratch)
	{
		assert(!isTempAcquired());
		m_useScratchTemp = _scratch;
	}

	void DspValue::setUseShiftTemp(const bool _shift)
	{
		assert(!isTempAcquired());
		m_useShiftTemp = _shift;
	}

	void DspValue::acquireTemp()
	{
		if(m_useScratchTemp)
			m_scratch.acquire();
		else if(m_usePooledTemp)
			m_pooledTemp.acquire();
		else if(m_useShiftTemp)
			m_shift.acquire();
		else
			m_gpTemp.acquire();
	}

	void DspValue::releaseTemp()
	{
		if (m_gpTemp.isValid())
		{
			assert(!m_usePooledTemp && !m_useScratchTemp && !m_useShiftTemp);
			m_gpTemp.release();
		}

		if(m_pooledTemp.acquired())
		{
			assert(m_usePooledTemp && !m_useScratchTemp && !m_useShiftTemp);
			m_pooledTemp.release();
		}

		if(m_scratch.isValid())
		{
			assert(!m_usePooledTemp && m_useScratchTemp && !m_useShiftTemp);
			m_scratch.release();
		}

		if(m_shift.isValid())
		{
			assert(!m_usePooledTemp && !m_useScratchTemp && m_useShiftTemp);
			m_shift.release();
		}
	}
}
