//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "tests/types.h"

#include <string>
#include <string_view>
#include <utility>

namespace slog_test {
    // The time of every record held at one instant: a manual clock
    // installed, its wall time put at `ns`
    struct FixedTime {
        explicit FixedTime(int64_t ns) {
            clock.install();
            sgcl::detail::manual_clock_wall_origin.store(ns, std::memory_order_relaxed);
        }

        async::manual_clock clock;
    };

    inline std::string text_of(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    // What f writes through a text logger and a JSON logger in UTC, from
    // any level
    template<class F>
    std::pair<std::string, std::string> both(F&& f) {
        io::buffer out;
        f(slog::logger(slog::options{.out = out, .level = slog::level(-100), .utc = true}));
        std::string text = text_of(out);
        out.clear();
        f(slog::logger(slog::options{.out = out, .level = slog::level(-100), .json = true, .utc = true}));
        return {text, text_of(out)};
    }
}
