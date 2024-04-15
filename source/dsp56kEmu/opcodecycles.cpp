#include "opcodecycles.h"

#include <iterator>

#include "opcodes.h"
#include "registers.h"

namespace dsp56k
{
	static_assert(std::size(g_cycles) == std::size(g_opcodes));

	namespace
	{
		constexpr bool validateCycles()
		{
			for(size_t i=0; i<std::size(g_cycles); ++i)
			{
				if(static_cast<size_t>(g_cycles[i].inst) != i)
					return false;
			}
			return true;
		}
	}

	static_assert(validateCycles());

	uint32_t calcCycles(const Instruction _inst, const TWord _pc, const TWord _op, const TWord _extMemAddress, const TWord _extMemWaitStates)
	{
		if(_inst == Invalid)
			return 0;

		const auto& cycles = g_cycles[static_cast<uint32_t>(_inst)];

		auto c = std::max(cycles.cycles, 1u);

		if(cycles.pru || cycles.lab || cycles.lim)
		{
			// pru: Pre-update specifies clock cycles added for using the pre-update addressing modes (pre-decrement and offset by N addressing modes)
			// lab: Long absolute specifies clock cycles added for using the Long Absolute Address

			TWord mmm = MMM_Rn;
			if(hasField(_inst, Field_MM))
				mmm = getFieldValue(_inst, Field_MM, _op);
			else if(hasField(_inst, Field_MMM))
				mmm = getFieldValue(_inst, Field_MMM, _op);
			else
				assert(false && "pru != 0 but no MMM field found");

			const auto eaMode = static_cast<EffectiveAddressingMode>(mmm);

			switch (eaMode)
			{
			case MMM_RnMinusNn:			// 000 (Rn)-Nn	
			case MMM_RnPlusNn:			// 001 (Rn)+Nn	
			case MMM_RnPlusNnNoUpdate:	// 101 (Rn+Nn)	
			case MMM_MinusRn:			// 111 -(Rn)
				c += cycles.pru;
				break;
			case MMM_AbsAddr:
				c += cycles.lab;
				c += cycles.lim;
				break;
			default:
				break;
			}
		}

		return c;
	}

	uint32_t calcCycles(const Instruction _instA, const Instruction _instB, const TWord _pc, const TWord _op, const TWord _extMemAddress, const TWord _extMemWaitStates)
	{
		const auto cyclesA = calcCycles(_instA, _pc, _op, _extMemAddress, _extMemWaitStates);
		const auto cyclesB = calcCycles(_instB, _pc, _op, _extMemAddress, _extMemWaitStates);

		return std::max(cyclesA, cyclesB);
	}
}
