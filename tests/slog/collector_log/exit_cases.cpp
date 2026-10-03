//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Records at the end of a program, run as a child by collector_log.cpp
// (TheEndOfAProgram), one case per argument:
// - static: a record from the destructor of a static made before the
//   default logger was first used, so destroyed after it;
// - buffered: the collector's lines through a buffered logger whose batch
//   hooks were set after collector_log's first call, so the exit writes
//   the batches before it drains the collector's last lines.
#define SGCL_LOG_PRINT_LEVEL 1
#include "sgcl/io/io.h"
#include "sgcl/slog/slog.h"

#include <cstring>

using namespace sgcl;

namespace {
    struct Late {
        bool armed = false;

        ~Late() {
            if (armed) {
                slog::info("late");
            }
        }
    };

    Late late;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    if (std::strcmp(argv[1], "static") == 0) {
        late.armed = true;
        slog::info("main");
        return 0;
    }
    if (std::strcmp(argv[1], "buffered") == 0) {
        slog::collector_log(slog::logger(io::stderr));
        auto log = slog::logger(slog::options{.out = io::stderr, .buffered = true});
        slog::collector_log(log);
        collector::force_collect(true);
        log.info("done");
        return 0;
    }
    return 2;
}
