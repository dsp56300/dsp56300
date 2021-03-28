#include "pch.h"

#include "registers.h"

namespace dsp56k
{
	const char* const g_regNames[Reg_COUNT] =
	{
		"x","y",		

		"a","b",		

		"x0","x1",		

		"y0",	"y1",		

		"a0",	"a1",	"a2",		

		"b0",	"b1",	"b2",		

		"pc",	"sr",	"omr",		

		"la",	"lc",	

		"ssh",	"ssl",	"sp",		

		"ep",	"sz",	"sc",	"vba",		

		"iprc",	"iprp",	"bcr",	"dcr",		

		"aar0",	"aar1",	"aar2",	"aar3",		

		"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",		

		"n0",	"n1",	"n2",	"n3",	"n4",	"n5",	"n6",	"n7",		

		"m0",	"m1",	"m2",	"m3",	"m4",	"m5",	"m6",	"m7",		

		"hit",		
		"miss",		
		"replace",	
		"cyc",		
		"ictr",		
		"cnt1",		
		"cnt2",		
		"cnt3",		
		"cnt4"
	};

	const size_t g_regBitCount[Reg_COUNT] = 
	{
		48,	48,

		56,	56,

		24,	24,

		24,	24,

		24,	24,	24,
		24,	24,	24,

		24,	24,	24,

		24,	24,
		24, 24,	24,
		24,	24,	24,	24,

		24,	24,	24,	24,
		24,	24,	24,	24,

		24,	24,	24,	24,	24,	24,	24,	24,

		24,	24,	24,	24,	24,	24,	24,	24,

		8,	8,	8,	8,	8,	8,	8,	8,

		24,	24,	24,	24,	24,	24,	24,	24,	24
	};
}
