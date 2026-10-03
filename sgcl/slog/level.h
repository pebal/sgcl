//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sgcl::slog {
    // The importance of a record, with Go's values, so that the levels
    // between them compare and read as Go's do: level(2) is written
    // INFO+2, level(-5) DEBUG-1. A logger writes the records at its level
    // and above.
    enum class level : int8_t {
        debug = -4,
        info = 0,
        warn = 4,
        error = 8
    };

    namespace detail {
        // The name of a level as slog writes it: the name of the nearest
        // named level at or below it, and the distance above that (or
        // below debug) as a signed number: "INFO", "WARN-1", "ERROR+5".
        // Into out (at least 12 bytes); the bytes written.
        inline size_t level_text(char* out, level l) noexcept {
            int v = int(l);
            const char* name;
            int base;
            if (v < int(level::info)) {
                name = "DEBUG";
                base = int(level::debug);
            } else if (v < int(level::warn)) {
                name = "INFO";
                base = int(level::info);
            } else if (v < int(level::error)) {
                name = "WARN";
                base = int(level::warn);
            } else {
                name = "ERROR";
                base = int(level::error);
            }
            size_t n = 0;
            while (name[n]) {
                out[n] = name[n];
                ++n;
            }
            int d = v - base;
            if (d != 0) {
                out[n++] = d < 0 ? '-' : '+';
                unsigned a = unsigned(d < 0 ? -d : d);
                char digits[4];
                int k = 0;
                do {
                    digits[k++] = char('0' + a % 10);
                    a /= 10;
                } while (a);
                while (k) {
                    out[n++] = digits[--k];
                }
            }
            return n;
        }

        // The door of the module's own code to the insides of its public
        // types (a logger's state, a level_var's word, a record's data)
        struct Access;

        struct LevelVarState {
            explicit LevelVarState(level l) noexcept
            : value(int8_t(l)) {
            }

            std::atomic<int8_t> value;
        };
    }

    // A level that changes while the program runs, shared by every logger
    // made with it (slog's LevelVar): options::level_var. A handle of one
    // word, made at its level by the constructor; the copies share it. A
    // set() is seen by the next record of every such logger, on any
    // thread (a relaxed atomic: a record in flight may still be judged by
    // the level before).
    class level_var {
    public:
        explicit level_var(level l = level::info) noexcept
        : _s(make_tracked<detail::LevelVarState>(l)) {
        }

        void set(level l) const noexcept {
            _s->value.store(int8_t(l), std::memory_order_relaxed);
        }

        level get() const noexcept {
            return level(_s->value.load(std::memory_order_relaxed));
        }

    private:
        friend struct detail::Access;

        tracked_ptr<detail::LevelVarState> _s;
    };
}
