//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The program of docs/sgcl/slog/README.md ("The collector's log"), run as a
// child by collector_log.cpp (TheExitWritesTheLastLines): the collector's
// last lines ("terminate collector", "stop collector") come after main's
// thread_locals are destroyed, and the exit writes them. The records went
// into the main thread's freed line (logger.h: Scratch) and the program
// ended on SIGTRAP or SIGABRT in about a third of the runs.
#define SGCL_LOG_PRINT_LEVEL 1
#include "sgcl/slog/slog.h"

using namespace sgcl;

int main() {
    slog::collector_log();
    collector::force_collect(true);
    slog::info("done");
}
