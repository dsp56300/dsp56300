#pragma once

#include <array>
#include <list>
#include <vector>

#include "jitdspregpoolregpair.h"
#include "types.h"

#include "jithelper.h"
#include "opcodeanalysis.h"

namespace asmjit
{
	inline namespace _abi_1_9
	{
		class BaseNode;
	}
}

namespace dsp56k
{
	class DspValue;
	class JitBlock;

	class JitDspRegPool
	{
	public:
		friend class JitRegPoolRegPair;

		struct SpillReg
		{
			JitReg128 reg;
			uint32_t offset = 0;

			void reset()
			{
				reg.reset();
				offset = 0;
			}
			bool isValid() const
			{
				return reg.isValid();
			}
			bool equals(const SpillReg& _r) const
			{
				return offset == _r.offset && reg.equals(_r.reg);
			}
		};

		JitDspRegPool(JitBlock& _block);
		~JitDspRegPool();

		JitRegGP get(PoolReg _reg, bool _read, bool _write)
		{
			return get(JitRegGP(), _reg, _read, _write);
		}
		JitRegGP get(const JitRegGP& _dst, PoolReg _reg, bool _read, bool _write);

		DspValue read(PoolReg _src) const;
		void read(const JitRegGP& _dst, PoolReg _src);
		void write(PoolReg _dst, const JitRegGP& _src);
		void write(PoolReg _dst, const DspValue& _src);

		void lock(PoolReg _reg);
		void unlock(PoolReg _reg);

		std::vector<PoolReg> lock(RegisterMask _read, RegisterMask _write);
		void unlock(const std::vector<PoolReg>& _regs);

		void releaseNonLocked();
		void releaseAll();
		void releaseWritten();
		void releaseLoaded();
		void releaseByFlags(DspRegFlags _flags);

		void discard(PoolReg _reg);

		JitBlock& getBlock() { return m_block; }

		void debugStoreAll();

		bool hasWrittenRegs() const { return m_writtenDspRegs != DspRegFlags::None; }

		DspRegFlags getLoadedRegs() const { return m_loadedDspRegs; }
		DspRegFlags getWrittenRegs() const { return m_writtenDspRegs; }
		DspRegFlags getSpilledRegs() const { return m_spilledDspRegs; }

		void setRepMode(bool _repMode) { m_repMode = _repMode; }

		bool isInUse(const JitReg128& _xmm) const;
		bool isInUse(const JitRegGP& _gp) const;
		bool isInUse(PoolReg _reg) const;

		PoolReg aquireTemp();
		void releaseTemp(PoolReg _reg);

		bool isWritten(PoolReg _reg) const		{ return flagTest(m_writtenDspRegs, _reg); }
		bool isLocked(PoolReg _reg) const		{ return flagTest(m_lockedGps, _reg); }

		bool move(PoolReg _dst, PoolReg _src);
		bool move(const JitRegGP& _dst, PoolReg _src);

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

		void movDspReg(const TReg5& _dst, const DspValue& _src) const;
		void movDspReg(DspValue& _dst, const TReg5& _src) const;

		void movDspReg(const TReg24& _dst, const DspValue& _src) const;
		void movDspReg(DspValue& _dst, const TReg24& _src) const;

		template<typename T, unsigned int B>
		void movDspReg(const RegType<T, B>& _reg, const JitRegGP& _src) const
		{
			static_assert(sizeof(_reg.var) == sizeof(uint64_t) || sizeof(_reg.var) == sizeof(uint32_t) || sizeof(_reg.var) == sizeof(uint8_t), "unknown register size");

			mov(makeDspPtr(_reg), _src);
		}

		template<typename T, unsigned int B>
		void movDspReg(const RegType<T, B>& _reg, const SpillReg& _src) const
		{
			if constexpr (sizeof(_reg.var) == sizeof(uint32_t))
				movd(makeDspPtr(_reg), _src);
			else if constexpr (sizeof(_reg.var) == sizeof(uint64_t))
				movq(makeDspPtr(_reg), _src);
			static_assert(sizeof(_reg.var) == sizeof(uint64_t) || sizeof(_reg.var) == sizeof(uint32_t), "unknown register size");
		}

		void movDspReg(const TWord& _reg, const SpillReg& _src) const
		{
			movd(makeDspPtr(&_reg, sizeof(_reg)), _src);
		}

		void movDspReg(const TWord& _reg, const JitRegGP& _src) const
		{
			mov(makeDspPtr(&_reg, sizeof(_reg)), r32(_src));
		}

		void movDspReg(const int8_t& _reg, const JitRegGP& _src) const;

		template<typename T, unsigned int B>
		void movDspReg(const JitRegGP& _dst, const RegType<T, B>& _reg) const
		{
			static_assert(sizeof(_reg.var) == sizeof(uint64_t) || sizeof(_reg.var) == sizeof(uint32_t) || sizeof(_reg.var) == sizeof(uint8_t), "unknown register size");
			mov(_dst, makeDspPtr(_reg));
		}

		void movDspReg(const JitRegGP& _dst, const TWord& _reg) const
		{
			mov(r32(_dst), makeDspPtr(&_reg, sizeof(_reg)));
		}

		void movDspReg(const JitRegGP& _dst, const int8_t& _reg) const;

		template<typename T, unsigned int B>
		JitMemPtr makeDspPtr(const RegType<T, B>& _reg) const
		{
			return makeDspPtr(&_reg, sizeof(_reg.var));
		}

		JitMemPtr makeDspPtr(const void* _ptr, size_t _size) const;

		void reset();

		RegisterMask toRegisterMask(PoolReg _reg, bool _writeAccess);

		void getXY(const JitReg64& _dst, uint32_t _xy);
		void getXY0(const JitReg32& _dst, uint32_t _xy);
		void getXY1(const JitReg32& _dst, uint32_t _xy);

		void setXY0(uint32_t _xy, const DspValue& _src);
		void setXY1(uint32_t _xy, const DspValue& _src);

	private:
		JitRegPoolRegPair& getPair(const bool _y)
		{
			return _y ? m_pairY : m_pairX;
		}
		void parallelOpEpilog(PoolReg _aluReadReg, PoolReg _aluWriteReg);
		
		void makeSpace(PoolReg _wantedReg);
		void clear();

		void load(const JitRegGP& _dst, PoolReg _src);
		void store(PoolReg _dst, const JitRegGP& _src) const;
		void store(PoolReg _dst, const SpillReg& _src) const;

		bool release(PoolReg _dst);

		template<typename T> void push(std::list<T>& _dst, const T& _src)
		{
			if(m_repMode)
				_dst.push_front(_src);
			else
				_dst.push_back(_src);
		}

		void setLoaded(PoolReg _reg)				{ flagSet(m_loadedDspRegs, _reg); }
		void clearLoaded(PoolReg _reg)			{ flagClear(m_loadedDspRegs, _reg); }

		void setWritten(PoolReg _reg)			{ m_dirty |= flagSet(m_writtenDspRegs, _reg); }
		void clearWritten(PoolReg _reg)			{ m_dirty |= flagClear(m_writtenDspRegs, _reg); }

		void setLocked(PoolReg _reg)				{ flagSet(m_lockedGps, _reg); }
		void clearLocked(PoolReg _reg)			{ flagClear(m_lockedGps, _reg); }

		void setSpilled(PoolReg _reg)			{ flagSet(m_spilledDspRegs, _reg); }
		void clearSpilled(PoolReg _reg)			{ flagClear(m_spilledDspRegs, _reg); }

		static bool flagSet(DspRegFlags& _flags, PoolReg _reg);
		static bool flagClear(DspRegFlags& _flags, PoolReg _reg);
		static bool flagTest(const DspRegFlags& _flags, PoolReg _reg);

		template<typename T> class RegisterList
		{
		public:
			RegisterList()
			{
				m_used.reserve(DspCount);
				m_available.reserve(DspCount);
			}

			bool isFull() const					{ return m_available.empty(); }
			bool isUsed(PoolReg _reg) const		{ return m_usedMap[_reg].isValid(); }
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

			bool acquire(T& _dst, const PoolReg _reg, bool _pushFront)
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

			bool get(T& _dst, const PoolReg _reg)
			{
				_dst = m_usedMap[_reg];
				return _dst.isValid();
			}

			bool release(T& _dst, PoolReg _reg, bool _pushFront)
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

			const std::vector<PoolReg>& used() const { return m_used; }

		private:
			template<typename TT> static void push(std::vector<TT>& _dst, const TT& _src, bool _pushFront)
			{
				if(_pushFront)
					_dst.insert(_dst.begin(), _src);
				else
					_dst.push_back(_src);
			}
			template<typename TT> static void remove(std::vector<TT>& _dst, const PoolReg _src)
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
			std::vector<PoolReg> m_used;
			T m_usedMap[DspCount];
		};

		void mov (const JitMemPtr& _dst, const JitRegGP& _src) const;
		void movd(const JitMemPtr& _dst, const SpillReg& _src) const;
		void movq(const JitMemPtr& _dst, const SpillReg& _src) const;

		void mov (const JitRegGP& _dst , const JitMemPtr& _src) const;
		void movd(const JitReg128& _dst, const JitMemPtr& _src) const;
		void movq(const JitReg128& _dst, const JitMemPtr& _src) const;

		void spillMove(const JitRegGP& _dst, const SpillReg& _src) const;
		void spillMove(const SpillReg& _dst, const JitRegGP& _src) const;
		void spillMove(const SpillReg& _dst, const SpillReg& _src) const;

		JitBlock& m_block;

		DspRegFlags m_lockedGps = DspRegFlags::None;
		DspRegFlags m_writtenDspRegs = DspRegFlags::None;
		DspRegFlags m_loadedDspRegs = DspRegFlags::None;
		DspRegFlags m_spilledDspRegs = DspRegFlags::None;

		RegisterList<JitRegGP> m_gpList;
		RegisterList<SpillReg> m_xmList;
		std::array<asmjit::BaseNode*, DspCount> m_moveToXmmInstruction{};

		std::vector<PoolReg> m_availableTemps;

		const bool m_extendedSpillSpace;
		bool m_isParallelOp = false;
		bool m_repMode = false;
		mutable JitMemPtr m_dspPtr;
		bool m_dirty = false;

		JitRegPoolRegPair m_pairX, m_pairY;
	};
}
