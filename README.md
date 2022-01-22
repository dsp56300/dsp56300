# Motorola DSP 56300 family emulator

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
An example project can be used to see how it works: The Chameleon Emulator.
The [SoundArt Chameleon](https://www.chameleon.synth.net/english/index.shtml) was a 19" rack unit that allowed anyone to develop their own audio algorithms for the Motorola DSP 56303. The SDK is well documented and comes with several examples, including DSP code.

The Chameleon Emulator wraps the SDK, compiles to VST2 and runs the DSP code in this emulator instead of the real device. All knobs of the unit are exposed as VST parameters. [Visit the Chameleon Emulator project](https://github.com/Lyve1981/chameleonEmulator/) if you want to know more.
It can currently run the dspthru example in polling mode and in interrupt mode.

### Join us on Discord

A core team of several people now work together to make the emulator do something more *"useful"* 😊
Contributions are welcome! If you want to help or just want to follow the state of the project, feel free to join us on Discord: https://discord.gg/WJ9cxySnsM

### Visit our Homepage

🎧We have some audio & video examples ready!🎵
Visit our homepage:
[DSP 56300 Emulation Blog](https://dsp56300.wordpress.com/)
