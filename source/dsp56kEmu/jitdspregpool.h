#pragma once

#include <list>
#include <vector>

#include "types.h"

#include "jithelper.h"

namespace dsp56k
{
	class JitBlock;

	class JitDspRegPool
	{
	public:
		enum DspReg
		{
			DspR0,	DspR1,	DspR2,	DspR3,	DspR4,	DspR5,	DspR6,	DspR7,
			DspN0,	DspN1,	DspN2,	DspN3,	DspN4,	DspN5,	DspN6,	DspN7,
			DspM0,	DspM1,	DspM2,	DspM3,	DspM4,	DspM5,	DspM6,	DspM7,

			DspA,		DspB,
			DspAwrite,	DspBwrite,

			DspX,	DspY,

			DspPC,
			DspSR,
			DspLC,
			DspLA,

			TempA, TempB, TempC, TempD, TempE, TempF, TempG, TempH, LastTemp = TempH,

			DspM0mod, DspM1mod, DspM2mod, DspM3mod, DspM4mod, DspM5mod, DspM6mod, DspM7mod,
			DspM0mask, DspM1mask, DspM2mask, DspM3mask, DspM4mask, DspM5mask, DspM6mask, DspM7mask,

			DspCount
		};

		JitDspRegPool(JitBlock& _block);
		~JitDspRegPool();

		JitRegGP get(DspReg _reg, bool _read, bool _write);

		void read(const JitRegGP& _dst, DspReg _src);
		void write(DspReg _dst, const JitRegGP& _src);

		void lock(DspReg _reg);
		void unlock(DspReg _reg);

		void releaseAll();
		void releaseWritten();

		bool hasWrittenRegs() const { return m_writtenDspRegs != 0; }

		void setRepMode(bool _repMode) { m_repMode = _repMode; }
		bool isInUse(const JitReg128& _xmm) const;
		bool isInUse(const JitRegGP& _gp) const;
		bool isInUse(DspReg _reg) const;

		DspReg aquireTemp();
		void releaseTemp(DspReg _reg);

		bool isWritten(DspReg _reg) const
		{
			return m_writtenDspRegs & (1ull<<static_cast<uint64_t>(_reg));
		}
		bool isLocked(DspReg _reg) const
		{
			return m_lockedGps & (1ull<<static_cast<uint64_t>(_reg));
		}

		bool move(DspReg _dst, DspReg _src);
		bool move(const JitRegGP& _dst, DspReg _src);

		void setIsParallelOp(bool _isParallelOp);

		bool isParallelOp() const
		{
			return m_isParallelOp;
		}

		void parallelOpEpilog();

		void resetDirty()
		{
			m_dirty = false;
		}

		bool isDirty() const
		{
			return m_dirty;
		}


		template<typename T, unsigned int B>
		void movDspReg(const RegType<T, B>& _reg, const JitRegGP& _src) const
		{
			if constexpr (sizeof(_reg.var) == sizeof(uint32_t))
				mov(makeDspPtr(_reg), r32(_src));
			else if constexpr (sizeof(_reg.var) == sizeof(uint64_t))
				mov(makeDspPtr(_reg), r64(_src));
			else if constexpr (sizeof(_reg.var) == sizeof(uint8_t))
				movb(makeDspPtr(_reg), r64(_src));
			static_assert(sizeof(_reg.var) == sizeof(uint64_t) || sizeof(_reg.var) == sizeof(uint32_t) || sizeof(_reg.var) == sizeof(uint8_t), "unknown register size");
		}

		template<typename T, unsigned int B>
		void movDspReg(const RegType<T, B>& _reg, const JitReg128& _src) const
		{
			if constexpr (sizeof(_reg.var) == sizeof(uint32_t))
				movd(makeDspPtr(_reg), _src);
			else if constexpr (sizeof(_reg.var) == sizeof(uint64_t))
				movq(makeDspPtr(_reg), _src);
			static_assert(sizeof(_reg.var) == sizeof(uint64_t) || sizeof(_reg.var) == sizeof(uint32_t), "unknown register size");
		}

		void movDspReg(const TWord& _reg, const JitReg128& _src) const
		{
			movd(makeDspPtr(&_reg, sizeof(_reg)), _src);
		}

		void movDspReg(const TWord& _reg, const JitRegGP& _src) const
		{
			mov(makeDspPtr(&_reg, sizeof(_reg)), r32(_src));
		}

		void movDspReg(const int8_t& _reg, const JitRegGP& _src) const
		{
			movb(makeDspPtr(&_reg, sizeof(_reg)), r32(_src));
		}

		template<typename T, unsigned int B>
		void movDspReg(const JitRegGP& _dst, const RegType<T, B>& _reg) const
		{
			if constexpr (sizeof(_reg.var) == sizeof(uint32_t))
				mov(r32(_dst), makeDspPtr(_reg));
			else if constexpr (sizeof(_reg.var) == sizeof(uint64_t))
				mov(r64(_dst), makeDspPtr(_reg));
			else if constexpr (sizeof(_reg.var) == sizeof(uint8_t))
				movb(_dst, makeDspPtr(_reg));
			static_assert(sizeof(_reg.var) == sizeof(uint64_t) || sizeof(_reg.var) == sizeof(uint32_t) || sizeof(_reg.var) == sizeof(uint8_t), "unknown register size");
		}

		void movDspReg(const JitRegGP& _dst, const TWord& _reg) const
		{
			mov(r32(_dst), makeDspPtr(&_reg, sizeof(_reg)));
		}

		void movDspReg(const JitRegGP& _dst, const int8_t& _reg) const
		{
			movb(r32(_dst), makeDspPtr(&_reg, sizeof(_reg)));
		}

		template<typename T, unsigned int B>
		JitMemPtr makeDspPtr(const RegType<T, B>& _reg) const
		{
			return makeDspPtr(&_reg, sizeof(_reg.var));
		}

		JitMemPtr makeDspPtr(const void* _ptr, size_t _size) const;

	private:
		void parallelOpEpilog(DspReg _aluReadReg, DspReg _aluWriteReg);
		
		void makeSpace(DspReg _wantedReg);
		void clear();

		void load(JitRegGP& _dst, DspReg _src);
		void store(DspReg _dst, const JitRegGP& _src) const;
		void store(DspReg _dst, const JitReg128& _src) const;

		bool release(DspReg _dst);

		template<typename T> void push(std::list<T>& _dst, const T& _src)
		{
			if(m_repMode)
				_dst.push_front(_src);
			else
				_dst.push_back(_src);
		}

		void setWritten(DspReg _reg);
		void clearWritten(DspReg _reg);

		void setLocked(DspReg _reg)				{ m_lockedGps |= (1ull<<static_cast<uint64_t>(_reg)); }											  
		void clearLocked(DspReg _reg)			{ m_lockedGps &= ~(1ull<<static_cast<uint64_t>(_reg)); }

		template<typename T> class RegisterList
		{
		public:
			RegisterList()
			{
				m_used.reserve(DspCount);
				m_available.reserve(DspCount);
			}

			bool isFull() const					{ return m_available.empty(); }
			bool isUsed(DspReg _reg) const		{ return m_usedMap[_reg].isValid(); }
			bool isUsed(const T& _reg) const
			{
				for(size_t i=0; i<DspCount; ++i)
				{
					if(m_usedMap[i].equals(_reg))
						return true;
				}
				return false;
			}
			bool empty() const					{ return m_used.empty(); }
			size_t available() const			{ return m_available.size(); }
			size_t size() const { return m_used.size(); }

			bool acquire(T& _dst, const DspReg _reg, bool _pushFront)
			{
				const auto existing = m_usedMap[_reg];

				if(existing.isValid())
				{
					if(!_pushFront)
					{
						remove(m_used, _reg);
						m_used.push_back(_reg);
					}
					_dst = existing;
					return true;
				}

				if(isFull())
					return false;

				auto res = m_available.front();
				m_available.erase(m_available.begin());
				m_usedMap[_reg] = res;

				push(m_used, _reg, _pushFront);
				_dst = res;
				return true;
			}

			bool get(T& _dst, const DspReg _reg)
			{
				_dst = m_usedMap[_reg];
				return _dst.isValid();
			}

			bool release(T& _dst, DspReg _reg, bool _pushFront)
			{
				_dst = m_usedMap[_reg];
				if(!_dst.isValid())
					return false;
				push(m_available, _dst, _pushFront);
				remove(m_used, _reg);
				m_usedMap[_reg].reset();
				return true;
			}

			void addHostReg(const T& _hr)
			{
				m_available.push_back(_hr);
			}

			void clear()
			{
				m_available.clear();
				m_used.clear();
				for(size_t i=0; i<DspCount; ++i)
					m_usedMap[i].reset();
			}

			const std::vector<DspReg>& used() const { return m_used; }

		private:
			template<typename TT> static void push(std::vector<TT>& _dst, const TT& _src, bool _pushFront)
			{
				if(_pushFront)
					_dst.insert(_dst.begin(), _src);
				else
					_dst.push_back(_src);
			}
			template<typename TT> static void remove(std::vector<TT>& _dst, const DspReg _src)
			{
				for(auto it = _dst.begin(); it != _dst.end(); ++it)
				{
					if(*it == _src)
					{
						_dst.erase(it);
						break;
					}
				}
			}
			std::vector<T> m_available;
			std::vector<DspReg> m_used;
			T m_usedMap[DspCount];
		};

		void mov (const JitMemPtr& _dst, const JitRegGP& _src) const;
		void movd(const JitMemPtr& _dst, const JitReg128& _src) const;
		void movq(const JitMemPtr& _dst, const JitReg128& _src) const;
		void movb(const JitMemPtr& _dst, const JitRegGP& _src) const;

		void mov (const JitRegGP& _dst , const JitMemPtr& _src) const;
		void movd(const JitReg128& _dst, const JitMemPtr& _src) const;
		void movq(const JitReg128& _dst, const JitMemPtr& _src) const;
		void movb(const JitRegGP& _dst, const JitMemPtr& _src) const;

		JitBlock& m_block;

		uint64_t m_lockedGps;
		uint64_t m_writtenDspRegs;

		RegisterList<JitRegGP> m_gpList;
		RegisterList<JitReg128> m_xmList;

		std::vector<DspReg> m_availableTemps;

		bool m_isParallelOp = false;
		bool m_repMode = false;
		mutable JitMemPtr m_dspPtr;
		bool m_dirty = false;
	};
}
