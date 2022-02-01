# Motorola DSP 56300 family emulator

### Emulation of the Motorola/Freescale/NXP 56300 family DSP

This DSP has been used in plenty of virtual analogue synthesizers and other musical gear that was released after around the mid 90s, such as Access Virus A, B, C, TI / Clavia Nord Lead 3 / Waldorf Q, Microwave II / Novation Supernova, Nova and many others.

The emulator should compile just fine on any platform that supports C++17, no configure is needed as the code uses C++17 standard data types. For performance reasons, it makes excessive use of C++17 features, for example to parse opcode definitions at compile time and to create jump tables of template permutations, so C++17 is a strong requirement.

The build system used is [cmake](https://cmake.org/).

### License

The Open-Source version of this emulator is licensed as GPL v3, i.e. any derivative work must be open source, too.

If you want to use this emulator for commercial projects, plese get in touch. A closed source derivative work exists that does static recompilation of existing DSP 56300 code and is available for purchase.

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

### Emulation of Access Virus B & C

One derivative work emulates the Access Virus B & C synthesizers, this emulator project is used to execute the DSP code from the original synthesizer ROMs.

If you want to follow the state of the project, join on Discord: https://discord.gg/WJ9cxySnsM

Or visit the homepage with several audio & video examples, VST and AU plugins and more: [DSP 56300 Emulation Blog](https://dsp56300.wordpress.com/)

The project source code can be found here: [gearmulator](https://github.com/dsp56300/gearmulator/)
