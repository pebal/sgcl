//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The collector's lines through slog (DESIGN 283, Q7): a program of its own,
// built with SGCL_LOG_PRINT_LEVEL=3 (the macro is the whole program's). The
// first case runs before the sink is installed and reads std::cout as it
// always read (the lines of benchmarks/heap/heap_parse.py); then
// collector_log(memory) and force_collect give records, a cycle's line as
// its numbers; then threads that log and collect at once (TSan); then the
// program of the docs run as a child, 200 times, for the lines of its exit.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <regex>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace {
    // What std::cout gets while f runs, through a pipe under its descriptor
    template<class F>
    std::string stdout_of(F&& f) {
        std::fflush(stdout);
        std::cout.flush();
        int pipe_fd[2];
        EXPECT_EQ(::pipe(pipe_fd), 0);
        const int saved = ::dup(1);
        ::dup2(pipe_fd[1], 1);
        f();
        std::cout.flush();
        std::fflush(stdout);
        ::dup2(saved, 1);
        ::close(saved);
        ::close(pipe_fd[1]);
        std::string out;
        char buf[4096];
        ssize_t n;
        while ((n = ::read(pipe_fd[0], buf, sizeof buf)) > 0) {
            out.append(buf, size_t(n));
        }
        ::close(pipe_fd[0]);
        return out;
    }

    std::string text_of(const slog::value& v) {
        auto t = v.text();
        return std::string(t.data(), t.size());
    }

    std::string attr(const slog::record& r, std::string_view key) {
        for (auto a : r) {
            if (std::string_view(a.key().data(), a.key().size()) == key) {
                return text_of(a.value());
            }
        }
        return "<none>";
    }
}

// Before the sink: the lines on std::cout, as they always were, in the form
// heap_parse.py reads
TEST(SlogCollectorLog_Tests, WithoutTheSinkTheLinesGoToStdout) {
    const std::string out = stdout_of([] {
        collector::force_collect(true);
    });
    EXPECT_NE(out.find("[sgcl] force collect and wait from id: "), std::string::npos) << out;
    std::regex cycle(R"(\[sgcl\] mem allocs:\s*(\d+),.*objects created:\s*(\d+))");
    EXPECT_TRUE(std::regex_search(out, cycle)) << out;
}

TEST(SlogCollectorLog_Tests, WithTheSinkTheLinesAreRecords) {
    slog::memory kept;
    slog::collector_log(slog::logger(kept));
    const std::string out = stdout_of([&] {
        collector::force_collect(true);
        slog::logger(kept).info("after");   // a record takes the waiting lines first
    });
    EXPECT_EQ(out.find("[sgcl]"), std::string::npos) << out;   // nothing on std::cout any more
    auto records = kept.records();
    bool forced = false, cycle = false, after = false;
    for (const auto& r : records) {
        const std::string_view msg(r.message().data(), r.message().size());
        if (msg == "after") {
            after = true;
            continue;
        }
        ASSERT_EQ(msg, "collector");
        EXPECT_EQ(r.level(), slog::level::info);
        if (attr(r, "line").rfind("force collect and wait from id: ", 0) == 0) {
            forced = true;
            EXPECT_EQ(attr(r, "verbosity"), "1");
        }
        if (attr(r, "mem_allocs") != "<none>") {
            cycle = true;
            EXPECT_EQ(attr(r, "verbosity"), "2");
            const std::string kind = attr(r, "cycle");
            EXPECT_TRUE(kind == "full" || kind == "young") << kind;
            EXPECT_NE(attr(r, "time_ms"), "<none>");
            EXPECT_NE(attr(r, "live_objects"), "<none>");
        }
    }
    EXPECT_TRUE(forced);
    EXPECT_TRUE(cycle);
    EXPECT_TRUE(after);
}

// Threads that register (level 3 lines from Thread's constructor), log and
// collect at once, and a buffered logger as the target
TEST(SlogCollectorLog_Tests, ThreadsThatLogAndCollect) {
    slog::memory kept;
    slog::collector_log(slog::logger(kept));
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&kept, t] {
            auto lg = slog::logger(kept);
            for (int i = 0; i < 50; ++i) {
                lg.info("work", "t", t, "i", i);
                if (i % 10 == 0) {
                    collector::force_collect(false);
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    collector::force_collect(true);
    slog::logger(kept).flush();
    size_t work = 0, from_collector = 0;
    for (const auto& r : kept.records()) {
        const std::string_view msg(r.message().data(), r.message().size());
        work += msg == "work";
        from_collector += msg == "collector";
    }
    EXPECT_EQ(work, 200u);
    EXPECT_GT(from_collector, 0u);
    slog::collector_log(slog::logger(io::discard));   // the memory handler let go of
}

// A target whose handler throws (DESIGN 408): the batch of lines it was
// given is lost, the record that took them goes on, and the next lines
// come; the target moved from is the same target
TEST(SlogCollectorLog_Tests, ATargetThatThrows) {
    struct Throws {
        std::atomic<int>* calls;
        std::atomic<bool>* fail;

        void handle(const slog::record&) const {
            ++*calls;
            if (fail->exchange(false)) {
                throw std::runtime_error("the handler threw");
            }
        }
    };
    std::atomic<int> calls{0};
    std::atomic<bool> fail{true};
    auto target = slog::logger(Throws{&calls, &fail});
    auto moved = std::move(target);
    slog::collector_log(target);
    slog::memory kept;
    const std::string out = stdout_of([&] {
        collector::force_collect(true);
        slog::logger(kept).info("after the throw");   // takes the lines; the throw stays inside
        EXPECT_EQ(kept.size(), 1u);
        const int before = calls.load();
        EXPECT_GE(before, 1);
        collector::force_collect(true);
        slog::logger(kept).info("again");
        EXPECT_GT(calls.load(), before);   // the next lines came
    });
    EXPECT_EQ(out.find("[sgcl]"), std::string::npos) << out;
    slog::collector_log(slog::logger(io::discard));
}

namespace {
    // One run of exit_program: its stderr, and whether it ended by itself
    // with 0 (a child that hangs is killed after 30 s and counts as failed)
    struct ExitRun {
        bool clean = false;
        std::string err;
        std::string how;
    };

    ExitRun run_child(const char* program, const char* argument) {
        ExitRun run;
        int pipe_fd[2];
        if (::pipe(pipe_fd) != 0) {
            run.how = "pipe";
            return run;
        }
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);   // the collector's start, said before the sink
        posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], 2);
        posix_spawn_file_actions_addclose(&actions, pipe_fd[0]);
        posix_spawn_file_actions_addclose(&actions, pipe_fd[1]);
        std::string path(program);
        std::string arg(argument ? argument : "");
        char* argv[] = {path.data(), argument ? arg.data() : nullptr, nullptr};
        pid_t pid = 0;
        const int spawned = ::posix_spawn(&pid, path.c_str(), &actions, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
        ::close(pipe_fd[1]);
        if (spawned != 0) {
            ::close(pipe_fd[0]);
            run.how = "posix_spawn: " + std::to_string(spawned);
            return run;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        bool killed = false;
        char buf[4096];
        for (;;) {
            pollfd p{pipe_fd[0], POLLIN, 0};
            if (::poll(&p, 1, 100) > 0) {
                const ssize_t n = ::read(pipe_fd[0], buf, sizeof buf);
                if (n <= 0) {
                    break;
                }
                run.err.append(buf, size_t(n));
            } else if (std::chrono::steady_clock::now() > deadline) {
                ::kill(pid, SIGKILL);
                killed = true;
                break;
            }
        }
        ::close(pipe_fd[0]);
        int status = 0;
        ::waitpid(pid, &status, 0);
        if (killed) {
            run.how = "hung, killed";
        } else if (WIFSIGNALED(status)) {
            run.how = "signal " + std::to_string(WTERMSIG(status));
        } else if (WEXITSTATUS(status) != 0) {
            run.how = "exit " + std::to_string(WEXITSTATUS(status));
        } else {
            run.clean = true;
        }
        return run;
    }
}

// The docs' program (exit_program.cpp) as a child, 200 times: the
// collector's last lines come when main's thread_locals are already
// destroyed (core/detail/thread.h: the main thread's Holder stops the
// collector there), and the exit writes them as records. Each run ends by
// itself with 0 and writes only whole records, the two last lines among
// them; the records used to go into the freed line of the main thread and
// the program ended on a signal, or wrote garbage first.
TEST(SlogCollectorLog_Tests, TheExitWritesTheLastLines) {
    const std::regex record(R"(time=\S+ level=INFO msg=(done|collector verbosity=1 line="(start collector id: |force collect and wait from id: |terminate collector from id: |stop collector id: )[0-9a-fx]+"))");   // the id as std::thread::id writes it
    constexpr int Runs = 200;
    int failed = 0;
    std::string first;
    for (int i = 0; i < Runs; ++i) {
        const ExitRun run = run_child(SGCL_SLOG_EXIT_PROGRAM, nullptr);
        bool ok = run.clean;
        bool done = false, terminate = false, stop = false;
        size_t at = 0;
        while (ok && at < run.err.size()) {
            const size_t end = run.err.find('\n', at);
            if (end == std::string::npos) {
                ok = false;   // a line cut short
                break;
            }
            const std::string line = run.err.substr(at, end - at);
            at = end + 1;
            ok = std::regex_match(line, record);
            done |= line.find("msg=done") != std::string::npos;
            terminate |= line.find("terminate collector from id: ") != std::string::npos;
            stop |= line.find("stop collector id: ") != std::string::npos;
        }
        if (!(ok && done && terminate && stop)) {
            ++failed;
            if (first.empty()) {
                first = "run " + std::to_string(i) + " (" + (run.how.empty() ? "clean exit" : run.how) + "):\n" + run.err;
            }
        }
    }
    EXPECT_EQ(failed, 0) << failed << " of " << Runs << " runs failed; the first:\n" << first;
}

// Records at the very end of a program (exit_cases.cpp, DESIGN 408), 20
// runs each. A record from the destructor of a static destroyed after the
// default logger read the default's freed word and hung or crashed; the
// collector's last lines through a buffered logger whose batch hooks came
// after collector_log's first call went into a batch the exit had already
// written, and were lost.
TEST(SlogCollectorLog_Tests, TheEndOfAProgram) {
    constexpr int Runs = 20;
    int failed = 0;
    std::string first;
    for (const char* which : {"static", "buffered"}) {
        for (int i = 0; i < Runs; ++i) {
            const ExitRun run = run_child(SGCL_SLOG_EXIT_CASES, which);
            bool ok = run.clean;
            if (std::string(which) == "static") {
                ok = ok && run.err.find("level=INFO msg=main\n") != std::string::npos && run.err.find("level=INFO msg=late\n") != std::string::npos;
            } else {
                ok = ok && run.err.find("msg=done\n") != std::string::npos && run.err.find("line=\"terminate collector from id: ") != std::string::npos
                     && run.err.find("line=\"stop collector id: ") != std::string::npos;
            }
            if (!ok) {
                ++failed;
                if (first.empty()) {
                    first = std::string(which) + " run " + std::to_string(i) + " (" + (run.how.empty() ? "clean exit" : run.how) + "):\n" + run.err;
                }
            }
        }
    }
    EXPECT_EQ(failed, 0) << failed << " runs failed; the first:\n" << first;
}
