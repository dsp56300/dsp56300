#pragma once

#include <list>
#include <map>
#include <set>

#include "jitregtypes.h"
#include "dspassert.h"

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

			DspA,	DspB,
			DspX,	DspY,

			DspExtMem,
			DspSR,
			DspLC,
			DspLA,
			DspCount
		};

		JitDspRegPool(JitBlock& _block);
		~JitDspRegPool();

		JitReg get(DspReg _reg, bool _read, bool _write);

		void read(const JitReg& _dst, DspReg _src);
		void write(DspReg _dst, const JitReg& _src);

		void lock(DspReg _reg);
		void unlock(DspReg _reg);

		void releaseAll();
		void releaseWritten();

		bool hasWrittenRegs() const { return !m_writtenDspRegs.empty(); }

		void setRepMode(bool _repMode) { m_repMode = _repMode; }
		bool isInUse(const JitReg128& _xmm) const;
		bool isInUse(const JitReg& _gp) const;

	private:
		void makeSpace(DspReg _wantedReg);
		void clear();

		void load(JitReg& _dst, DspReg _src);
		void store(DspReg _dst, JitReg& _src);
		void store(DspReg _dst, JitReg128& _src);

		bool release(DspReg _dst);

		template<typename T> class RegisterList
		{
		public:
			bool isFull() const					{ return m_available.empty(); }
			bool isUsed(DspReg _reg) const		{ return m_usedMap.find(_reg) != m_usedMap.end(); }
			bool isUsed(const T& _reg) const
			{
				for(auto it = m_usedMap.begin(); it != m_usedMap.end(); ++it)
				{
					if(it->second.equals(_reg))
						return true;
				}
				return false;
			}
			bool empty() const					{ return m_used.empty(); }
			size_t available() const			{ return m_available.size(); }
			size_t size() const { return m_used.size(); }

			bool acquire(T& _dst, const DspReg _reg, bool _pushFront)
			{
				const auto itExisting = m_usedMap.find(_reg);

				if(itExisting != m_usedMap.end())
				{
					if(!_pushFront)
					{
						m_used.remove(_reg);
						m_used.push_back(_reg);
					}
					_dst = itExisting->second;
					return true;
				}

				if(isFull())
					return false;

				auto res = m_available.front();
				m_available.pop_front();
				m_usedMap.insert(std::make_pair(_reg, res));

				push(m_used, _reg, _pushFront);
				_dst = res;
				return true;
			}

			bool get(T& _dst, const DspReg _reg)
			{
				const auto it = m_usedMap.find(_reg);
				if(it == m_usedMap.end())
					return false;
				_dst = it->second;
				return true;
			}

			bool release(T& _dst, DspReg _reg, bool _pushFront)
			{
				const auto it = m_usedMap.find(_reg);
				if(it == m_usedMap.end())
					return false;
				_dst = it->second;
				push(m_available, it->second, _pushFront);
				m_used.remove(_reg);
				m_usedMap.erase(it);
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
				m_usedMap.clear();
			}

			const std::list<DspReg>& used() const { return m_used; }

		private:
			template<typename T> void push(std::list<T>& _dst, const T& _src, bool _pushFront)
			{
				if(_pushFront)
					_dst.push_front(_src);
				else
					_dst.push_back(_src);
			}
			std::list<T> m_available;
			std::list<DspReg> m_used;
			std::map<DspReg, T> m_usedMap;
		};

		JitBlock& m_block;

		std::set<DspReg> m_lockedGps;
		std::set<DspReg> m_writtenDspRegs;

		RegisterList<JitReg> m_gpList;
		RegisterList<JitReg128> m_xmList;

		bool m_repMode = false;
	};
}
