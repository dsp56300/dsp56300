// Where do p99 spikes come from? Per 128-frame block, record wall time and DSP instructions executed, optionally at
// SCHED_FIFO priority (MNM_FIFO=<prio>) pinned to a CPU (MNM_CPU=<n>), then compare the slowest 1% of blocks with the rest.
// MNM_PACE=1 sleeps until each block's deadline like a real audio thread. Without it, a SCHED_FIFO thread that never
// sleeps hits the kernel's RT throttling (sched_rt_runtime_us 950000 per 1 s on the Force), which shows up as a ~40 ms
// burst of ~1.5x-slow blocks once per wall-clock second -- a benchmark artifact, not something an audio thread sees.
//   mnm-spikes <os.syx> [seconds=3] [machine-substring]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include <sched.h>
#include <thread>
#include "MonoVoice.h"
#include "firmware/Firmware.h"
#include "host/Machines.h"

using namespace mnm;
using Clock = std::chrono::steady_clock;
constexpr int kFrames = 128;

int main(int argc, char** argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: mnm-spikes <os.syx> [seconds] [machine]\n"); return 2; }
    if (const char* c = std::getenv("MNM_CPU")) { cpu_set_t s; CPU_ZERO(&s); CPU_SET(std::atoi(c), &s); sched_setaffinity(0, sizeof(s), &s); }
    if (const char* p = std::getenv("MNM_FIFO")) {
        sched_param sp{}; sp.sched_priority = std::atoi(p);
        if (sched_setscheduler(0, SCHED_FIFO, &sp)) std::perror("sched_setscheduler");
    }
    const auto fw = fw::loadFirmware(argv[1]);
    const double seconds = argc > 2 ? std::atof(argv[2]) : 3.0;
    const char* only = argc > 3 ? argv[3] : nullptr;
    const int blocks = int(seconds * 44100.0 / kFrames);
    const double deadline = 1e6 * kFrames / 44100.0;
    std::vector<float> L(kFrames), R(kFrames), inL(kFrames), inR(kFrames);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> noise(-0.5f, 0.5f);
    std::printf("%-11s %7s %7s %7s | %9s %9s | %-6s | %s\n", "machine", "mean%", "p99%", "max%", "instr/blk", "p99instr", "slow1%",
                "p99% over 256/512/1024-frame windows");
    for (const auto& def : host::kMachineDefs) {
        if (only && !std::strstr(def.name, only)) continue;
        MonoVoice v(fw);
        auto& h = v.host();
        h.setMachine(def.machine);
        const bool fx = host::isFxMachine(def.machine);
        h.setRouting(fx ? host::dspInputBits(host::FxInput::InpAB) : 0u);
        v.warmUp(8);
        h.noteOn(fx ? 60 : 48);
        std::vector<double> us; std::vector<double> ins;
        const bool pace = std::getenv("MNM_PACE") != nullptr;
        auto next = Clock::now();
        for (int b = 0; b < blocks; ++b) {
            if (fx) for (int i = 0; i < kFrames; ++i) { inL[i] = noise(rng); inR[i] = noise(rng); }
            const uint64_t i0 = v.engine().stats().totalInstructions;
            const auto s = Clock::now();
            if (fx) v.processFx(inL.data(), inR.data(), L.data(), R.data(), kFrames);
            else v.process(L.data(), R.data(), kFrames);
            us.push_back(std::chrono::duration<double, std::micro>(Clock::now() - s).count());
            ins.push_back(double(v.engine().stats().totalInstructions - i0));
            if (!fx && b == blocks / 2) h.noteOn(55);
            if (pace) {
                next += std::chrono::microseconds(int64_t(deadline));
                std::this_thread::sleep_until(next);
            }
        }
        if (const char* dump = std::getenv("MNM_DUMP")) {   // per-block microseconds, one per line
            if (FILE* f = std::fopen(dump, "w")) { for (double x : us) std::fprintf(f, "%.1f\n", x); std::fclose(f); }
        }
        std::vector<size_t> idx(us.size()); for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return us[a] < us[b]; });
        auto sortedIns = ins; std::sort(sortedIns.begin(), sortedIns.end());
        double mean = 0, meanIns = 0; for (size_t i = 0; i < us.size(); ++i) { mean += us[i]; meanIns += ins[i]; }
        mean /= us.size(); meanIns /= us.size();
        const size_t k = us.size() / 100 ? us.size() / 100 : 1;
        double slowIns = 0; for (size_t i = us.size() - k; i < us.size(); ++i) slowIns += ins[idx[i]]; slowIns /= k;
        // a host buffer of W blocks only has to finish W blocks in W deadlines: p99 of non-overlapping W-block sums
        auto windowP99 = [&](size_t w) {
            std::vector<double> sums;
            for (size_t i = 0; i + w <= us.size(); i += w) { double t = 0; for (size_t j = 0; j < w; ++j) t += us[i + j]; sums.push_back(t / w); }
            std::sort(sums.begin(), sums.end());
            return 100 * sums[size_t(sums.size() * 0.99)] / deadline;
        };
        std::printf("%-11s %6.1f%% %6.1f%% %6.1f%% | %9.0f %9.0f | %.2fx  | %5.1f%% %5.1f%% %5.1f%%\n", def.name, 100 * mean / deadline,
                    100 * us[idx[size_t(us.size() * 0.99)]] / deadline, 100 * us[idx.back()] / deadline, meanIns,
                    sortedIns[size_t(sortedIns.size() * 0.99)], slowIns / meanIns, windowP99(2), windowP99(4), windowP99(8));
    }
    return 0;
}
