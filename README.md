# Motorola DSP 56300 family emulator

### Emulation of the Motorola/Freescale/NXP 56300 family DSP

This DSP has been used in plenty of virtual analogue synthesizers and other musical gear that was released after around the mid 90s, such as Access Virus A, B, C, TI / Clavia Nord Lead 3 / Waldorf Q, Microwave II / Novation Supernova, Nova and many others.

The emulator should compile just fine on any platform that supports C++14, no configure is needed as the code uses C++14 standard data types. For performance reasons, it makes excessive use of C++14 features, for example to parse opcode definitions at compile time, so C++14 is a strong requirement.

The build system used is [cmake](https://cmake.org/).

### Development

For now, to be able to test the current state of the emulation, I develop the emulator in another project, which is a wrapper of the Chameleon SDK. The [SoundArt Chameleon](https://www.chameleon.synth.net/english/index.shtml) was a 19" rack unit that allowed anyone to develop their own audio algorithms for the Motorola DSP 56303. The SDK is well documented and comes with several examples, including DSP code.

The Chameleon Emulator wraps the SDK, compiles to VST2 and runs the DSP code in this emulator instead of the real device. All knobs of the unit are exposed as VST parameters. [Visit the Chameleon Emulator project](https://github.com/Lyve1981/chameleonEmulator/) if you want to know more.
It can currently run the dspthru example in polling mode and in interrupt mode. DMA will one of the next steps. The goal is to run all included examples without issues, including the [MonoWave II](https://www.chameleon.synth.net/english/skins/monowave2/).

### Performance

Performance goal: Emulating the DSP56362 in real time, which runs at 120MHz, resulting in 120 million instructions/second. The current speed varies and improves daily, on a Core i7 4790K @ 4 GHz the emulated speed at the moment reaches about 42 MHz, i.e. 42 million instructions/second, but there is still enough potential to improve emulation speed.

### Join us on Discord

I ceased development of this emulator about 10 years ago but due to [this reddit post](https://www.reddit.com/r/synthesizers/comments/l29ys5/dsp563xx_emulator_access_virus_nord_lead_waldorf/) I resumed working on it and a core team of several people now work together to make the emulator do something *"useful"* 😊
Contributions are welcome! If you want to help or just want to follow the state of the project, feel free to join us on Discord: https://discord.gg/mveFUNbNCK
