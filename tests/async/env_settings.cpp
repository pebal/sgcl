//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The settings of the environment, each read once where it is first needed
// (sgcl/core/detail/env.h): a program of its own, run by CTest with the
// variables set before it starts (tests/CMakeLists.txt, the ENVIRONMENT of
// each env_settings_* test), since a setenv inside a program that has
// started comes too late to show anything. The first argument says what
// the environment holds:
//
//   workers    SGCL_WORKERS=3 SGCL_WORKER_SPIN_US=5 SGCL_BLOCKING_THREADS=7:
//              the scheduler starts with 3 workers and a spin of 5 us, the
//              pool's cap is 7; the program's own calls win afterwards
//   explicit   the same environment, and the program's calls before the
//              first task: they win (2 workers, a spin of 9 us, a cap of 4)
//   memory N   SGCL_MEMORY_LIMIT=64M: the heap's ceiling is N bytes;
//              set_memory_limit wins
//   percent P  SGCL_MEMORY_LIMIT=50%: the ceiling is P % of the memory the
//              process may use; set_memory_limit_percent wins
//   invalid    SGCL_WORKERS=abc SGCL_MEMORY_LIMIT=12Q: both ignored (a line
//              on stderr each), the defaults taken, the program runs
//
// Exit 0 when every check holds.
#include "sgcl/async/blocking.h"
#include "sgcl/async/scheduler.h"
#include "sgcl/core/collector.h"
#include "sgcl/core/detail/os.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {
    int failures = 0;

    void expect(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "env_settings: FAILED %s\n", what);
            ++failures;
        }
    }

    sgcl::async::task<int> twice(int v) {
        co_return 2 * v;
    }

    unsigned default_workers() {
        return std::min(64u, std::max(1u, std::thread::hardware_concurrency()));
    }
}

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    namespace async = sgcl::async;
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "workers") {
        expect(async::scheduler::workers() == 3, "SGCL_WORKERS=3 gives 3 workers");
        expect(async::scheduler::worker_spin() == 5us, "SGCL_WORKER_SPIN_US=5 gives a spin of 5 us");
        expect(async::blocking_pool::max_threads() == 7, "SGCL_BLOCKING_THREADS=7 gives a cap of 7");
        expect(async::spawn(twice(21)).wait() == 42, "a task runs");
        async::scheduler::set_workers(2);
        expect(async::scheduler::workers() == 2, "set_workers after the start wins over the environment");
        async::blocking_pool::set_threads(9);
        expect(async::blocking_pool::max_threads() == 9, "set_threads wins over the environment");
        expect(async::spawn(twice(4)).wait() == 8, "a task runs after the restart");
    } else if (mode == "explicit") {
        async::scheduler::set_workers(2);
        async::scheduler::set_worker_spin(9us);
        async::blocking_pool::set_threads(4);
        expect(async::scheduler::workers() == 2, "set_workers before the first task wins over SGCL_WORKERS");
        expect(async::scheduler::worker_spin() == 9us, "set_worker_spin before the start wins over SGCL_WORKER_SPIN_US");
        expect(async::blocking_pool::max_threads() == 4, "set_threads before the first job wins over SGCL_BLOCKING_THREADS");
        expect(async::spawn(twice(5)).wait() == 10, "a task runs");
    } else if (mode == "memory" && argc > 2) {
        const size_t want = size_t(std::strtoull(argv[2], nullptr, 10));
        expect(sgcl::collector::get_memory_limit() == want, "SGCL_MEMORY_LIMIT gives the ceiling in bytes");
        sgcl::collector::set_memory_limit(100u << 20);
        expect(sgcl::collector::get_memory_limit() == (100u << 20), "set_memory_limit wins over the environment");
    } else if (mode == "percent" && argc > 2) {
        const size_t percent = size_t(std::strtoull(argv[2], nullptr, 10));
        const size_t base = sgcl::detail::os::memory_limit();
        expect(base != 0, "the system says how much memory there is");
        expect(sgcl::collector::get_memory_limit() == base / 100 * percent, "SGCL_MEMORY_LIMIT=P% gives P % of the base");
        sgcl::collector::set_memory_limit_percent(25);
        expect(sgcl::collector::get_memory_limit() == base / 100 * 25, "set_memory_limit_percent wins over the environment");
    } else if (mode == "invalid") {
        expect(async::scheduler::workers() == default_workers(), "SGCL_WORKERS=abc is ignored");
        expect(sgcl::collector::get_memory_limit() == sgcl::detail::os::memory_limit() / 100 * sgcl::config::heap_limit_percent, "SGCL_MEMORY_LIMIT=12Q is ignored");
        expect(async::spawn(twice(1)).wait() == 2, "a task runs");
    } else {
        std::fprintf(stderr, "env_settings: workers | explicit | memory N | percent P | invalid\n");
        return 2;
    }
    async::scheduler::stop();
    std::printf("env_settings %s: %s\n", mode.c_str(), failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
