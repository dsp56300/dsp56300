#include <iostream>

#include "dsp56kEmu/dspconfig.h"
#include "dsp56kEmu/assemblertest.h"
#include "dsp56kEmu/jitunittests.h"
#include "dsp56kEmu/jitoptimizertests.h"
#include "dsp56kEmu/interpreterunittests.h"

int main(int _argc, char* _argv[])
{
	std::cout << "Running Assembler Tests..." << std::endl;
	try
	{
		dsp56k::AssemblerTest assemblerTests;
	}
	catch(const std::string& _err)
	{
		std::cout << "Assembler test failed: " << _err << std::endl;
		return -1;
	}

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

	if (dsp56k::g_jitSupported)
	{
		std::cout << "Running JIT Optimizer Tests..." << std::endl;
		try
		{
			dsp56k::JitOptimizerTests optimizerTests;
		}
		catch(const std::string& _err)
		{
			std::cout << "JIT Optimizer test failed: " << _err << std::endl;
			return -1;
		}
		std::cout << "JIT Optimizer Tests finished." << std::endl;
	}

	return 0;
}
