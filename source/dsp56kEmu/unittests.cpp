#include "pch.h"

#include "unittests.h"

#include "dsp.h"
#include "memory.h"

namespace dsp56k
{
#define T true
#define F false

	UnitTests::UnitTests() : dsp(mem)
	{
		//                            <  <= =  >= >  != 
		testCCCC(0xff000000000000, 0, T, T, F, F, F, T);
		testCCCC(0x00ff0000000000, 0, F, F, F, T, T, T);
		testCCCC(0x00000000000000, 0, F, T, T, T ,F ,F);
	}

	void UnitTests::testCCCC(const int64_t _value, const int64_t _compareValue, const bool _lt, bool _le, bool _eq, bool _ge, bool _gt, bool _neq)
	{
		dsp.resetHW();
		dsp.reg.a.var = _value;
		dsp.alu_cmp(false, TReg56(_compareValue), false);
		char sr[16]{};
		dsp.sr_debug(sr);
		assert(_lt == dsp.decode_cccc(CCCC_LessThan));
		assert(_le == dsp.decode_cccc(CCCC_LessEqual));
		assert(_eq == dsp.decode_cccc(CCCC_Equal));
		assert(_ge == dsp.decode_cccc(CCCC_GreaterEqual));
		assert(_gt == dsp.decode_cccc(CCCC_GreaterThan));
		assert(_neq == dsp.decode_cccc(CCCC_NotEqual));	
	}
}
