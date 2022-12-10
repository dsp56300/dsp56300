#pragma once

#include <array>
#include <list>
#include <vector>

#include "types.h"

#include "jithelper.h"

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
		enum DspReg
		{
			DspRegInvalid = -1,

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

		enum class DspRegFlags : uint64_t
		{
			None = 0,

			R0 = 1ull<<DspR0,	R1 = 1ull<<DspR1,	R2 = 1ull<<DspR2,	R3 = 1ull<<DspR3,	R4 = 1ull<<DspR4,	R5 = 1ull<<DspR5,	R6 = 1ull<<DspR6,	R7 = 1ull<<DspR7,
			N0 = 1ull<<DspN0,	N1 = 1ull<<DspN1,	N2 = 1ull<<DspN2,	N3 = 1ull<<DspN3,	N4 = 1ull<<DspN4,	N5 = 1ull<<DspN5,	N6 = 1ull<<DspN6,	N7 = 1ull<<DspN7,
			M0 = 1ull<<DspM0,	M1 = 1ull<<DspM1,	M2 = 1ull<<DspM2,	M3 = 1ull<<DspM3,	M4 = 1ull<<DspM4,	M5 = 1ull<<DspM5,	M6 = 1ull<<DspM6,	M7 = 1ull<<DspM7,

			A = 1ull<<DspA,			B = 1ull<<DspB,
			Awrite = 1ull<<DspAwrite,	Bwrite = 1ull<<DspBwrite,

			X = 1ull<<DspX,	Y= 1ull<<DspY,

			PC = 1ull<<DspPC,
			SR = 1ull<<DspSR,
			LC = 1ull<<DspLC,
			LA = 1ull<<DspLA,

			T0 = 1ull<<TempA, T1 = 1ull<<TempB, T2 = 1ull<<TempC, T3 = 1ull<<TempD, T4 = 1ull<<TempE, T5 = 1ull<<TempF, T6 = 1ull<<TempG, T7 = 1ull<<TempH,

			M0mod  = 1ull<<DspM0mod,  M1mod  = 1ull<<DspM1mod,  M2mod  = 1ull<<DspM2mod,  M3mod  = 1ull<<DspM3mod,  M4mod  = 1ull<<DspM4mod,  M5mod  = 1ull<<DspM5mod,  M6mod  = 1ull<<DspM6mod,  M7mod  = 1ull<<DspM7mod,
			M0mask = 1ull<<DspM0mask, M1mask = 1ull<<DspM1mask, M2mask = 1ull<<DspM2mask, M3mask = 1ull<<DspM3mask, M4mask = 1ull<<DspM4mask, M5mask = 1ull<<DspM5mask, M6mask = 1ull<<DspM6mask, M7mask = 1ull<<DspM7mask,
		};

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

		JitRegGP get(DspReg _reg, bool _read, bool _write)
		{
			return get(JitRegGP(), _reg, _read, _write);
		}
		JitRegGP get(const JitRegGP& _dst, DspReg _reg, bool _read, bool _write);

		DspValue read(DspReg _src) const;
		void read(const JitRegGP& _dst, DspReg _src);
		void write(DspReg _dst, const JitRegGP& _src);
		void write(DspReg _dst, const DspValue& _src);

		void lock(DspReg _reg);
		void unlock(DspReg _reg);

		void releaseNonLocked();
		void releaseAll();
		void releaseWritten();
		void releaseLoaded();
		void releaseByFlags(DspRegFlags _flags);

		void debugStoreAll();

		bool hasWrittenRegs() const { return m_writtenDspRegs != DspRegFlags::None; }

		DspRegFlags getLoadedRegs() const { return m_loadedDspRegs; }
		DspRegFlags getWrittenRegs() const { return m_writtenDspRegs; }
		DspRegFlags getSpilledRegs() const { return m_spilledDspRegs; }

		void setRepMode(bool _repMode) { m_repMode = _repMode; }

		bool isInUse(const JitReg128& _xmm) const;
		bool isInUse(const JitRegGP& _gp) const;
		bool isInUse(DspReg _reg) const;

		DspReg aquireTemp();
		void releaseTemp(DspReg _reg);

		bool isWritten(DspReg _reg) const		{ return flagTest(m_writtenDspRegs, _reg); }
		bool isLocked(DspReg _reg) const		{ return flagTest(m_lockedGps, _reg); }

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

	private:
		void parallelOpEpilog(DspReg _aluReadReg, DspReg _aluWriteReg);
		
		void makeSpace(DspReg _wantedReg);
		void clear();

		void load(const JitRegGP& _dst, DspReg _src);
		void store(DspReg _dst, const JitRegGP& _src) const;
		void store(DspReg _dst, const SpillReg& _src) const;

		bool release(DspReg _dst);

		template<typename T> void push(std::list<T>& _dst, const T& _src)
		{
			if(m_repMode)
				_dst.push_front(_src);
			else
				_dst.push_back(_src);
		}

		void setLoaded(DspReg _reg)				{ flagSet(m_loadedDspRegs, _reg); }
		void clearLoaded(DspReg _reg)			{ flagClear(m_loadedDspRegs, _reg); }

		void setWritten(DspReg _reg)			{ m_dirty |= flagSet(m_writtenDspRegs, _reg); }
		void clearWritten(DspReg _reg)			{ m_dirty |= flagClear(m_writtenDspRegs, _reg); }

		void setLocked(DspReg _reg)				{ flagSet(m_lockedGps, _reg); }
		void clearLocked(DspReg _reg)			{ flagClear(m_lockedGps, _reg); }

		void setSpilled(DspReg _reg)			{ flagSet(m_spilledDspRegs, _reg); }
		void clearSpilled(DspReg _reg)			{ flagClear(m_spilledDspRegs, _reg); }

		static bool flagSet(DspRegFlags& _flags, DspReg _reg);
		static bool flagClear(DspRegFlags& _flags, DspReg _reg);
		static bool flagTest(const DspRegFlags& _flags, DspReg _reg);

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

		std::vector<DspReg> m_availableTemps;

		const bool m_extendedSpillSpace;
		bool m_isParallelOp = false;
		bool m_repMode = false;
		mutable JitMemPtr m_dspPtr;
		bool m_dirty = false;
	};

	static constexpr JitDspRegPool::DspRegFlags operator | (const JitDspRegPool::DspRegFlags& _a, const JitDspRegPool::DspRegFlags& _b)
	{
		return static_cast<JitDspRegPool::DspRegFlags>(static_cast<uint64_t>(_a) | static_cast<uint64_t>(_b));
	}

	static constexpr JitDspRegPool::DspRegFlags operator & (const JitDspRegPool::DspRegFlags& _a, const JitDspRegPool::DspRegFlags& _b)
	{
		return static_cast<JitDspRegPool::DspRegFlags>(static_cast<uint64_t>(_a) & static_cast<uint64_t>(_b));
	}

	static constexpr JitDspRegPool::DspRegFlags operator ~ (const JitDspRegPool::DspRegFlags& _a)
	{
		return static_cast<JitDspRegPool::DspRegFlags>(~static_cast<uint64_t>(_a));
	}
}
