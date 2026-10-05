//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The end of a program with the async runtime busy (DESIGN 478): each case
// of exit/exit_cases.cpp as a child, 20 runs each. The runtime's singletons
// (the scheduler, the reactor, the timers, the blocking pool) are statics
// destroyed at exit in the reverse order of their construction; a task
// parked in the reactor when main returned was woken by the reactor's
// destructor and locked its destroyed mutex (a TLS listener's loop, almost
// every run), a reactor made by a worker during the exit died after the
// scheduler and locked the scheduler's, and a timer or a blocking job from a
// static's destructor after them locked theirs: "mutex lock failed:
// Invalid argument" out of a noexcept destructor, std::terminate.
#include "tests/types.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {
    struct ExitRun {
        bool clean = false;   // ended by itself with 0
        std::string how;      // otherwise: a signal, a status, a hang
        std::string err;      // what it wrote to stderr
    };

    // The child with stdout to /dev/null and stderr into a pipe, read
    // until it closes; killed after 30 s
    ExitRun run_child(const char* program, const char* argument) {
        ExitRun run;
        int pipe_fd[2];
        if (::pipe(pipe_fd) != 0) {
            run.how = "pipe";
            return run;
        }
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], 2);
        posix_spawn_file_actions_addclose(&actions, pipe_fd[0]);
        posix_spawn_file_actions_addclose(&actions, pipe_fd[1]);
        std::string path(program);
        std::string arg(argument);
        char* argv[] = {path.data(), arg.data(), nullptr};
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

    // Runs of a case that did not end cleanly, with stderr free of the
    // failures of a destroyed mutex and of std::terminate, and with the
    // line the case must write (empty: none)
    void expect_clean_exits(const char* which, const char* line) {
        constexpr int Runs = 20;
        int failed = 0;
        std::string first;
        for (int i = 0; i < Runs; ++i) {
            const ExitRun run = run_child(SGCL_ASYNC_EXIT_CASES, which);
            bool ok = run.clean && run.err.find("mutex lock failed") == std::string::npos && run.err.find("terminating") == std::string::npos;
            if (*line) {
                ok = ok && run.err.find(line) != std::string::npos;
            }
            if (!ok) {
                ++failed;
                if (first.empty()) {
                    first = "run " + std::to_string(i) + " (" + (run.how.empty() ? "clean exit" : run.how) + "):\n" + run.err;
                }
            }
        }
        EXPECT_EQ(failed, 0) << which << ": " << failed << " of " << Runs << " runs failed; the first:\n" << first;
    }
}

TEST(AsyncExit_Tests, AcceptLoopParkedAtExit) {
    expect_clean_exits("tcp_accept_loop", "");
}

TEST(AsyncExit_Tests, AcceptParkedAtExit) {
    expect_clean_exits("tcp_accept_once", "");
}

TEST(AsyncExit_Tests, TlsListenerAtExit) {
    expect_clean_exits("tls_listen", "");
}

TEST(AsyncExit_Tests, TlsListenerParkedAtExit) {
    expect_clean_exits("tls_listen_wait", "");
}

TEST(AsyncExit_Tests, HttpServeAtExit) {
    expect_clean_exits("http_serve", "");
}

TEST(AsyncExit_Tests, HttpsServeAtExit) {
    expect_clean_exits("https_serve", "");
}

TEST(AsyncExit_Tests, TimerAfterTeardown) {
    expect_clean_exits("timer_after_teardown", "timer after teardown: done\n");
}

TEST(AsyncExit_Tests, BlockingJobAfterTeardown) {
    expect_clean_exits("blocking_after_teardown", "blocking after teardown: 7\n");
}
