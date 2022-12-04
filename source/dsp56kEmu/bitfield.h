#pragma once

namespace dsp56k
{
	struct Bit
	{
		const uint32_t bit;

		explicit Bit(uint32_t _bit) : bit(_bit) {}
		template <uint32_t V> explicit constexpr Bit() : bit(V) {static_assert(V == 0 || V == 1); }

		Bit operator != (const Bit& _bit) const		{ return Bit(bit ^ _bit.bit); }
		Bit operator == (const Bit& _bit) const		{ return Bit((bit ^ _bit.bit) ^ 1);	}
		Bit operator | (const Bit& _bit) const		{ return Bit(bit | _bit.bit); }
		Bit operator ^ (const Bit& _bit) const		{ return Bit(bit ^ _bit.bit); }

		explicit operator bool() const				{ return bit; }
		Bit operator !() const						{ return Bit(bit^1); }
	};

	template<typename T, typename B, size_t C=24>
	class Bitfield
	{
	public:
		enum
		{
			bitCount = C,
		};

		explicit Bitfield(T _value = 0) : m_value(_value) {}

		T testMask(const B _mask) const							{ return m_value & _mask; }
		T test(const B _bit) const							{ return m_value & mask(_bit); }
		T test(const B _a, const B _b) const				{ return m_value & mask(_a,_b); }
		T test(const B _a, const B _b, const B _c) const	{ return m_value & mask(_a, _b, _c); }

		void clear(const B _bit)						{ m_value &= ~mask(_bit); }
		void clear(const B _a, const B _b)				{ m_value &= ~(mask(_a) | mask(_b)); }

		void set(const B _bit)							{ m_value |= mask(_bit); }
		void set(const B _a, const B _b)				{ m_value |= mask(_a) | mask(_b); }

		void toggle(const B _bit)						{ m_value ^= mask(_bit); }

		static constexpr T mask(const B _bit)
		{
			return (1<<_bit);
		}

		static T mask(const B _a, const B _b)
		{
			return mask(_a) | mask(_b);
		}

		static T mask(const B _a, const B _b, const B _c)
		{
			return mask(_a) | mask(_b) | mask(_c);
		}

		operator const T& () const
		{
			return m_value;
		}

		Bitfield<T,B,C>& operator = (T _value)
		{
			m_value = _value;
			return *this;
		}

	private:
		T m_value;
	};
}
