# Motorola DSP 56300 family emulator

[![CMake][s0]][l0] ![GPLv3][s1]

[s0]: https://github.com/dsp56300/dsp56300/actions/workflows/cmake.yml/badge.svg
[l0]: https://github.com/dsp56300/dsp56300/actions/workflows/cmake.yml

[s1]: https://img.shields.io/badge/license-GPLv3-blue.svg

### Emulation of the Motorola/Freescale/NXP 56300 family DSP

This DSP has been used in plenty of virtual analogue synthesizers and other musical gear that was released after around the mid 90s, such as Access Virus A, B, C, TI / Clavia Nord Lead 3 / Waldorf Q, Microwave II / Novation Supernova, Nova and many others.

The emulator should compile just fine on any platform that supports C++17, no configure is needed as the code uses C++17 standard data types. For performance reasons, it makes excessive use of C++17 features, for example to parse opcode definitions at compile time and to create jump tables of template permutations, so C++17 is a strong requirement.

The build system used is [cmake](https://cmake.org/).

### Development

Please not that this project is a generic DSP emulator and outputs nothing but a static library after building. To use it, you need to create a project on your own, which can be a command line app, a VST plugin or whatever and instantiate the DSP class and feed data into it.

Minimal example:
```c++
#include "../dsp56300/source/dsp56kEmu/dsp.h"

int main(int argc, char* argv[])
{
	// Create DSP memory
	constexpr TWord g_memorySize = 0x040000;

	const DefaultMemoryValidator memoryMap;
	Memory memory(memoryMap, g_memorySize);

	// External SRAM starts at 0x20000
	memory.setExternalMemory(0x020000, true);

	// TODO: Load useful data into memory like this
	// Example: write a nop to P memory at address $100
	// memory.set(MemArea_P, 0x100, 0x000000);

	// Use 56362 peripherals: ESAI, HDI08
	Peripherals56362 periph;

	// Instantiate DSP
	DSP dsp(memory, &periph, &periph);

	// set starting address
	dsp.setPC(0x100); 

	while(true)
	{
		// run forever
		dsp.exec();
	}
}
```
