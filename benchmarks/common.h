//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Shared bits of the benchmarks: timing, CPU time, percentiles. Every
// benchmark takes its variant as the first argument, so one binary holds
// them all and they are built with the same flags.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace bench {
    using Clock = std::chrono::steady_clock;

    inline double seconds_since(Clock::time_point t0) {
        return std::chrono::duration<double>(Clock::now() - t0).count();
    }

    // user + system time of the whole process: the collector's work shows
    // up here even though it runs on its own thread
    inline double cpu_seconds() {
#if defined(_WIN32)
        FILETIME c, e, k, u;
        ::GetProcessTimes(::GetCurrentProcess(), &c, &e, &k, &u);
        auto to_s = [](FILETIME t) { return ((uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime) * 1e-7; };
        return to_s(k) + to_s(u);
#else
        rusage r;
        ::getrusage(RUSAGE_SELF, &r);
        return r.ru_utime.tv_sec + r.ru_utime.tv_usec * 1e-6 + r.ru_stime.tv_sec + r.ru_stime.tv_usec * 1e-6;
#endif
    }

    inline unsigned hardware_threads() {
        auto n = std::thread::hardware_concurrency();
        return n ? n : 1;
    }

    inline bool has_variant(const char* name, std::initializer_list<const char*> known) {
        for (auto k : known) {
            if (!std::strcmp(name, k)) {
                return true;
            }
        }
        return false;
    }

    // Percentiles of a sample of nanoseconds, printed on one line.
    struct Percentiles {
        double p50, p90, p99, p999, p9999, max;
    };

    inline Percentiles percentiles(std::vector<uint32_t>& ns) {
        std::sort(ns.begin(), ns.end());
        auto at = [&](double q) { return ns.empty() ? 0.0 : double(ns[std::min(ns.size() - 1, size_t(q * ns.size()))]); };
        return {at(0.5), at(0.9), at(0.99), at(0.999), at(0.9999), ns.empty() ? 0.0 : double(ns.back())};
    }
}
