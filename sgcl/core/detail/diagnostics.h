//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// Where the lines of SGCL_LOG_PRINT_LEVEL go (core/config.h): to std::cout,
// byte for byte as they always went, or to a sink a module installs (slog's
// collector_log, DESIGN 283 Q7). The line is made where it is said, as
// before, and handed on whole; only its destination is decided here. A
// line comes from the collector's thread and from a thread's registration,
// where nothing managed may be touched: the sink takes plain bytes and
// must not allocate managed memory, block for long or throw. Included only
// where SGCL_LOG_PRINT_LEVEL is above 0.
namespace sgcl::detail {
    // The sink: the line's level (the SGCL_LOG_PRINT_LEVEL it needs, 1 to
    // 3), its bytes without the new line; null: std::cout
    using DiagnosticSink = void (*)(int level, const char* text, size_t n) noexcept;

    inline std::atomic<DiagnosticSink> diagnostic_sink = {nullptr};

    inline void diagnostic_line(int level, const std::string& line) noexcept {
        if (auto sink = diagnostic_sink.load(std::memory_order_acquire)) {
            sink(level, line.data(), line.size());
            return;
        }
        try {
            std::cout << line << std::endl;
        } catch (...) {
        }
    }

    // The id of the calling thread as std::cout writes it
    inline std::string diagnostic_thread_id() {
        std::ostringstream s;
        s << std::this_thread::get_id();
        return s.str();
    }
}
