// Stand-in for MPC's own audio work: one SCHED_RR thread per listed core that burns <duty>% of every
// 128-frame period (2902 us at 44.1 kHz) and sleeps until the next period, like MPC's AudioWorker threads
// (SCHED_RR, priority 20, one per core). Run it next to mnm-bench --engines to see how many Monomachine
// engines fit beside a given amount of other audio work.
// It also reports how often its burst finished after the end of its own period (a missed audio deadline for the
// thing it stands in for), which is how you see the cost to MPC of running a heavy real-time thread above it.
//   mnm-busyload <seconds> <duty%> <rr-prio> <cpu,cpu,...>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <sched.h>
using Clock = std::chrono::steady_clock;
int main(int argc, char** argv)
{
    if (argc < 5) { std::fprintf(stderr, "usage: mnm-busyload <seconds> <duty%%> <rr-prio> <cpu,cpu,...>\n"); return 2; }
    const double seconds = std::atof(argv[1]), duty = std::atof(argv[2]) / 100.0;
    const int prio = std::atoi(argv[3]);
    std::vector<int> cpus; for (const char* q = argv[4]; *q;) { cpus.push_back(std::atoi(q)); while (*q && *q != ',') ++q; if (*q) ++q; }
    const auto period = std::chrono::microseconds(2902);
    std::vector<std::thread> ts;
    std::vector<double> lateFrac(cpus.size()), worstLate(cpus.size());
    for (size_t k = 0; k < cpus.size(); ++k) ts.emplace_back([&, k] {
        const int c = cpus[k];
        cpu_set_t s; CPU_ZERO(&s); CPU_SET(c, &s); sched_setaffinity(0, sizeof(s), &s);
        // calibrate iterations per microsecond while uncontended (before going real-time)
        volatile unsigned x = 1;
        auto c0 = Clock::now(); unsigned long iters = 0;
        while (Clock::now() - c0 < std::chrono::milliseconds(150)) { for (int i = 0; i < 1000; ++i) x = x * 1664525u + 1013904223u; iters += 1000; }
        const double perUs = iters / 150000.0;
        if (prio > 0) { sched_param sp{}; sp.sched_priority = prio; if (sched_setscheduler(0, SCHED_RR, &sp)) std::perror("SCHED_RR"); }
        const unsigned long work = static_cast<unsigned long>(2902 * duty * perUs);
        const auto end = Clock::now() + std::chrono::milliseconds(int(seconds * 1000));
        auto next = Clock::now();
        size_t periods = 0, late = 0; double worst = 0;
        while (Clock::now() < end) {
            const auto start = Clock::now();
            for (unsigned long i = 0; i < work; ++i) x = x * 1664525u + 1013904223u;   // fixed amount of work, not fixed time
            const auto done = Clock::now();
            next += period;
            ++periods;
            const double over = std::chrono::duration<double, std::micro>(done - next).count();   // finished after the period ended
            if (over > 0) { ++late; worst = std::max(worst, over); next = done; }                   // resync after a miss
            std::this_thread::sleep_until(next);
            (void)start;
        }
        lateFrac[k] = periods ? 100.0 * late / periods : 0; worstLate[k] = worst;
    });
    for (auto& t : ts) t.join();
    for (size_t k = 0; k < cpus.size(); ++k)
        std::printf("busyload cpu%d duty %.0f%%: %.2f%% of periods finished late, worst %.0f us late\n", cpus[k], duty * 100, lateFrac[k], worstLate[k]);
    return 0;
}
