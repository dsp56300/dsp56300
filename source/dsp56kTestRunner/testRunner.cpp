#include <iostream>

#include "dsp56kEmu/jitunittests.h"
#include "dsp56kEmu/unittests.h"

int main(int _argc, char* _argv[])
{
	std::cout << "Running Unit Tests..." << std::endl;
//	UnitTests tests;
	dsp56k::JitUnittests jitTests;
	std::cout << "Unit Tests finished." << std::endl;

	return 0;
}
