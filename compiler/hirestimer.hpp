#pragma once

namespace ceramic {
struct HiResTimer {
    unsigned long long elapsedTicks;
    unsigned long long startTicks;
    int running;

    HiResTimer();
    void start();
    void stop();
    unsigned long long elapsedNanos();
    double elapsedMillis() { return (double)elapsedNanos() / (1000 * 1000); }
};

struct CeramicTimers {
    HiResTimer locate, read, parse, install, initMod;    // load sub-phases
    HiResTimer topLevel, externals, mainEntry, finalize; // compile sub-phases
};

extern CeramicTimers timers;
} // namespace ceramic
