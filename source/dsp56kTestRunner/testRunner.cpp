#include <iostream>

#include "dsp56kEmu/dspconfig.h"
#include "dsp56kEmu/jitunittests.h"
#include "dsp56kEmu/interpreterunittests.h"

int main(int _argc, char* _argv[])
{
	std::cout << "Running Unit Tests..." << std::endl;
	try
	{
		if (dsp56k::g_jitSupported)
			dsp56k::JitUnittests jitTests;
		else
			dsp56k::InterpreterUnitTests tests;
	}
	catch(const std::string& _err)
	{
		std::cout << "Unit tet failed: " << _err << std::endl;
		return -1;
	}
	std::cout << "Unit Tests finished." << std::endl;

	return 0;
}
