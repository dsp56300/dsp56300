#pragma once

#include <cstdint>
#include <cstring>

#include "jittypes.h"

namespace asmjit
{
	inline namespace ASMJIT_ABI_NAMESPACE
	{
		class BaseNode;
		class InstNode;
		struct InstRWInfo;
	}
}

namespace dsp56k
{
	class JitEmitter;

	class JitOptimizer
	{
	public:
		JitOptimizer(JitEmitter& _emitter);

		size_t optimize();

	private:
		// Register key: compact index into a flat array.
		// GP registers: 0-31, Vec registers: 32-63, CPU flags: 64
		static constexpr uint32_t kGpBase  = 0;
		static constexpr uint32_t kVecBase = 32;
		static constexpr uint32_t kFlagsIdx = 64;
		static constexpr uint32_t kRegCount = 65;

		static uint32_t regIndex(uint32_t _group, uint32_t _id);
		static uint32_t regIndex(const asmjit::Operand& _op, bool& _valid);

		// Bitset for register liveness — replaces std::set<RegKey>
		struct RegBits
		{
			uint64_t bits[2] = {};	// 128 bits, we use 65

			void clear()							{ bits[0] = 0; bits[1] = 0; }
			void set(uint32_t _idx)					{ bits[_idx >> 6] |= (1ull << (_idx & 63)); }
			void clr(uint32_t _idx)					{ bits[_idx >> 6] &= ~(1ull << (_idx & 63)); }
			bool test(uint32_t _idx) const			{ return (bits[_idx >> 6] & (1ull << (_idx & 63))) != 0; }
			bool empty() const						{ return bits[0] == 0 && bits[1] == 0; }
		};

		// Flat constant map — replaces std::map<RegKey, int64_t>
		struct ConstMap
		{
			int64_t values[kRegCount] = {};
			uint64_t valid[2] = {};	// bitfield: is slot occupied?

			void clear()									{ valid[0] = 0; valid[1] = 0; }
			bool has(uint32_t _idx) const					{ return (valid[_idx >> 6] & (1ull << (_idx & 63))) != 0; }
			int64_t get(uint32_t _idx) const				{ return values[_idx]; }
			void set(uint32_t _idx, int64_t _val)			{ values[_idx] = _val; valid[_idx >> 6] |= (1ull << (_idx & 63)); }
			void erase(uint32_t _idx)						{ valid[_idx >> 6] &= ~(1ull << (_idx & 63)); }

			// Intersect: keep only entries present in both with same value
			void intersect(const ConstMap& _other)
			{
				for(uint32_t i = 0; i < kRegCount; ++i)
				{
					if(!has(i)) continue;
					if(!_other.has(i) || _other.values[i] != values[i])
						erase(i);
				}
			}
		};

		// Small fixed-capacity map for label → ConstMap (replaces std::map<uint32_t, std::map<...>>)
		static constexpr uint32_t kMaxLabels = 32;
		struct LabelConstMap
		{
			uint32_t labelIds[kMaxLabels] = {};
			ConstMap maps[kMaxLabels];
			uint32_t count = 0;

			ConstMap* find(uint32_t _labelId)
			{
				for(uint32_t i = 0; i < count; ++i)
					if(labelIds[i] == _labelId) return &maps[i];
				return nullptr;
			}

			ConstMap& insert(uint32_t _labelId)
			{
				if(count < kMaxLabels)
				{
					labelIds[count] = _labelId;
					maps[count].clear();
					return maps[count++];
				}
				// overflow: reuse last slot (lossy but safe — just prevents folding)
				labelIds[kMaxLabels - 1] = _labelId;
				maps[kMaxLabels - 1].clear();
				return maps[kMaxLabels - 1];
			}

			void erase(uint32_t _labelId)
			{
				for(uint32_t i = 0; i < count; ++i)
				{
					if(labelIds[i] == _labelId)
					{
						labelIds[i] = labelIds[count - 1];
						maps[i] = maps[count - 1];
						--count;
						return;
					}
				}
			}
		};

		static int64_t maskToRegSize(const asmjit::Operand& _reg, int64_t _value);

		size_t deadCodeElimination() const;
		size_t constantFolding();

		bool isSideEffectFree(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo) const;
		bool isControlFlow(const asmjit::BaseNode* _node) const;
		bool areFlagsLive(const asmjit::BaseNode* _node) const;
		static bool writesFlags(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo);
		static bool readsFlags(const asmjit::InstNode* _inst, const asmjit::InstRWInfo& _rwInfo);

		JitEmitter& m_asm;
	};
}
