// Gearmulator-on-Force feasibility probe (arm32 study). One binary per synth, chosen by GM_SYNTH_*.
// usage: gm_probe <romdir> [seconds=20] [dump.raw]
// Runs a scripted workload in lock step (block-granular MIDI), prints an FNV hash of every output
// sample plus the interpreter's executed-instruction count and wall time.
#include <unistd.h>
#include <dirent.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <thread>

#include "synthLib/device.h"
#include "synthLib/romLoader.h"
#include "dsp56kEmu/dsp.h"

#if defined(GM_SYNTH_VIRUS)
#include "virusLib/device.h"
#include "virusLib/romloader.h"
#elif defined(GM_SYNTH_MQ)
#include "mqLib/device.h"
#include "mqLib/romloader.h"
#elif defined(GM_SYNTH_XT)
#include "xtLib/xtDevice.h"
#include "xtLib/xtRomLoader.h"
#elif defined(GM_SYNTH_N2X)
#include "n2xLib/n2xdevice.h"
#include "n2xLib/n2xromloader.h"
#endif

using namespace synthLib;
using clk = std::chrono::steady_clock;

static uint64_t g_hash = 1469598103934665603ull;
static void hashWord(uint32_t w) { g_hash ^= w; g_hash *= 1099511628211ull; }

int main(int argc, char** argv)
{
	const std::string dir = argc > 1 ? argv[1] : ".";
	const double seconds = argc > 2 ? atof(argv[2]) : 20.0;
	FILE* dump = argc > 3 ? fopen(argv[3], "wb") : nullptr;

	RomLoader::setSearchPath(dir);
	DeviceCreateParams p;
#if defined(GM_SYNTH_VIRUS)
	auto rom = virusLib::ROMLoader::findROM();
	if (!rom.isValid()) { puts("no virus rom"); return 2; }
	p.romName = rom.getFilename(); p.romData = rom.getRomFileData(); p.customData = static_cast<uint32_t>(rom.getModel());
	std::unique_ptr<Device> dev(new virusLib::Device(p));
#elif defined(GM_SYNTH_MQ)
	auto rom = mqLib::RomLoader::findROM();
	if (!rom.isValid()) { puts("no mq rom"); return 2; }
	p.romData = rom.getData(); p.romName = rom.getFilename();
	std::unique_ptr<Device> dev(new mqLib::Device(p));
#elif defined(GM_SYNTH_XT)
	auto rom = xt::RomLoader::findROM();
	if (!rom.isValid()) { puts("no xt rom"); return 2; }
	p.romData = rom.getData(); p.romName = rom.getFilename();
	std::unique_ptr<Device> dev(new xt::Device(p));
#elif defined(GM_SYNTH_N2X)
	auto rom = n2x::RomLoader::findROM();
	if (!rom.isValid()) { puts("no n2x rom"); return 2; }
	p.romData.assign(rom.data().begin(), rom.data().end()); p.romName = rom.getFilename();
	std::unique_ptr<Device> dev(new n2x::Device(p));
#endif
	if (!dev->isValid()) { puts("device invalid"); return 3; }

	const float sr = dev->getSamplerate();
	const uint32_t nOut = dev->getChannelCountOut();
	const size_t block = 64;
	std::vector<std::vector<float>> outBuf(12, std::vector<float>(block)), inBuf(4, std::vector<float>(block, 0.0f));
	TAudioInputs ins{}; TAudioOutputs outs{};
	for (size_t i = 0; i < 4; ++i) ins[i] = inBuf[i].data();
	for (size_t i = 0; i < 12; ++i) outs[i] = outBuf[i].data();

	std::vector<SMidiEvent> midiIn, midiOut;
	auto ev = [&](uint8_t a, uint8_t b, uint8_t c) { midiIn.emplace_back(MidiEventSource::Host, a, b, c, 0u); };

	const uint64_t blocks = static_cast<uint64_t>(seconds * sr / block);
	// the script: every 1.0 s a new program + chord + CC sweep, chord released after 0.7 s
	const uint64_t period = static_cast<uint64_t>(sr / block);
	static const uint8_t chord[] = {36, 48, 55, 60, 64, 67, 72, 79};
	static const uint8_t ccs[] = {1, 74, 71, 73, 72, 91, 93, 5, 7, 10};

	const uint64_t i0 = dsp56k::DSP::execCountAll();
	const auto t0 = clk::now();
	for (uint64_t b = 0; b < blocks; ++b)
	{
		midiIn.clear(); midiOut.clear();
		const uint64_t step = b / period, ph = b % period;
		if (ph == 0)
		{
			ev(0xb0, 0, static_cast<uint8_t>((step / 32) & 3));       // bank
			ev(0xc0, static_cast<uint8_t>((step * 7) & 127), 0);      // program
			for (int k = 0; k < 4 + static_cast<int>(step % 5); ++k)
				ev(0x90, chord[(k + step) % 8], static_cast<uint8_t>(60 + ((step * 13 + k * 17) % 60)));
		}
		if (ph > 4 && ph % 8 == 0)
		{
			const uint8_t cc = ccs[(step + ph / 8) % 10];
			ev(0xb0, cc, static_cast<uint8_t>(((ph * 3 + step * 29) * 5) & 127));
		}
		if (ph == period * 7 / 10)
			for (int k = 0; k < 8; ++k) ev(0x80, chord[k], 0);
		dev->process(ins, outs, block, midiIn, midiOut);
		for (size_t s = 0; s < block; ++s)
			for (uint32_t c = 0; c < nOut && c < 12; ++c)
			{
				uint32_t w; memcpy(&w, &outBuf[c][s], 4); hashWord(w);
				if (dump) fwrite(&outBuf[c][s], 4, 1, dump);
			}
	}
	const double wall = std::chrono::duration<double>(clk::now() - t0).count();
	const uint64_t instr = dsp56k::DSP::execCountAll() - i0;
	printf("synth=%s sr=%.0f out=%u audio=%.2fs wall=%.2fs rt=%.2fx exec_instr=%llu rate=%.2f Minstr/s hash=%016llx\n",
		GM_NAME, sr, nOut, blocks * block / sr, wall, wall / (blocks * block / sr),
		(unsigned long long)instr, instr / (blocks * block / sr) / 1e6, (unsigned long long)g_hash);
	{	// per-thread CPU seconds (whole run incl. boot)
		DIR* d = opendir("/proc/self/task");
		while (auto* e = d ? readdir(d) : nullptr)
		{
			if (e->d_name[0] == '.') continue;
			char path[128]; snprintf(path, sizeof path, "/proc/self/task/%s/stat", e->d_name);
			FILE* f = fopen(path, "r"); if (!f) continue;
			char buf[512]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
			char* cl = strrchr(buf, ')'); char* op = strchr(buf, '(');
			if (!cl || !op) continue;
			*cl = 0; unsigned long ut = 0, st = 0;
			sscanf(cl + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &st);
			printf("thread %-16s cpu=%.2fs\n", op + 1, (ut + st) / (double)sysconf(_SC_CLK_TCK));
		}
		if (d) closedir(d);
	}
	printf("spin_skipped=%llu\n", (unsigned long long)dsp56k::DSP::spinSkippedAll());
	if (getenv("GM_HOT")) dsp56k::DSP::dumpHotAll(static_cast<size_t>(atoi(getenv("GM_HOT"))));
	if (dump) fclose(dump);
	fflush(stdout);
	_exit(0);	// destructors can hang on a DSP that never reaches WAIT (interpreter build)
}
