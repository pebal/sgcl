//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The settings a program's environment may give the library at run time,
// each read once, where the value is first needed (as Go reads GOMAXPROCS
// and GOMEMLIMIT): SGCL_WORKERS, SGCL_WORKER_SPIN_US, SGCL_BLOCKING_THREADS
// (async: scheduler.h, blocking.h) and SGCL_MEMORY_LIMIT (the heap's
// ceiling: heap.h). A call of the program (scheduler::set_workers,
// collector::set_memory_limit, ...) wins over the environment, the
// environment over the constant of the build. A value that does not read
// is ignored, with one line on stderr, and the default taken: a setting
// of the environment never stops a program.
namespace sgcl::detail {
    // An unsigned number from the variable, or `fallback` when it is not
    // set or does not read (a line on stderr then)
    inline unsigned env_unsigned(const char* name, unsigned fallback) noexcept {
        const char* text = std::getenv(name);
        if (!text || !*text) {
            return fallback;
        }
        char* end = nullptr;
        errno = 0;
        unsigned long long value = std::strtoull(text, &end, 10);
        if (errno || end == text || *end || text[0] == '-' || value > 0xFFFFFFFFull) {
            std::fprintf(stderr, "[sgcl] %s=%s ignored: not a number\n", name, text);
            return fallback;
        }
        return unsigned(value);
    }

    // What a size in the environment says: bytes, or a percentage of a
    // base the caller knows
    struct EnvSize {
        bool set = false;           // the variable read
        bool percent = false;       // "50%": `value` is the percentage
        uint64_t value = 0;         // bytes, or the percentage (1..100)
    };

    // "536870912", "512K", "512M", "2G" (powers of 1024; lower case too),
    // or "50%" (1..100); anything else, and 0, is ignored with a line on
    // stderr
    inline EnvSize env_size_or_percent(const char* name) noexcept {
        EnvSize out;
        const char* text = std::getenv(name);
        if (!text || !*text) {
            return out;
        }
        char* end = nullptr;
        errno = 0;
        unsigned long long value = std::strtoull(text, &end, 10);
        bool ok = !errno && end != text && text[0] != '-' && value != 0;
        uint64_t scale = 1;
        bool percent = false;
        if (ok && *end) {
            switch (*end) {
                case 'k': case 'K': scale = uint64_t(1) << 10; break;
                case 'm': case 'M': scale = uint64_t(1) << 20; break;
                case 'g': case 'G': scale = uint64_t(1) << 30; break;
                case '%': percent = true; break;
                default: ok = false; break;
            }
            if (ok && end[1]) {
                ok = false;   // one suffix, nothing after it
            }
        }
        if (ok && percent && value > 100) {
            ok = false;
        }
        if (ok && !percent && value > ~uint64_t(0) / scale) {
            ok = false;
        }
        if (!ok) {
            std::fprintf(stderr, "[sgcl] %s=%s ignored: bytes (K, M, G) or a percentage (1%%..100%%)\n", name, text);
            return out;
        }
        out.set = true;
        out.percent = percent;
        out.value = percent ? value : value * scale;
        return out;
    }
}
