//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// exec at its boundaries (DESIGN 408): a child that ends before its pipes
// are read or written, a call out of order (wait before start, a second
// start or wait), names that are no program, empty arguments and an empty
// environment, a stream that fails while it is copied, the default state
// and process.
#include "tests/types.h"

using namespace sgcl::async;

#include <csignal>
#include <string>
#include <sys/wait.h>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;

    struct IoExecBounds_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            auto d = make_temp_dir({}, "sgcl-exec-bounds-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            io::remove_all(string(_dir));
        }

        string at(const char* name) const {
            return path::join(string(_dir), string(name));
        }
    };

    // The child's end waited for, the child left to be reaped by the
    // command's wait (WNOWAIT): it has exited, its pipes not yet read
    bool ended(int pid) {
        siginfo_t info{};
        return ::waitid(P_PID, id_t(pid), &info, WEXITED | WNOWAIT) == 0;
    }

    // A writer that fails at once, counting what it was asked to write
    class refusing final : public io::mixin::writer<refusing> {
    public:
        using io::mixin::writer<refusing>::write;

        expected<size_t, io::error> write(const slice<const byte>& data) {
            asked += data.size();
            return sgcl::unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "refusing"));
        }

        task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            co_return write(data);
        }

        size_t asked = 0;
    };
}

// A child that has exited before the program reads its output pipe: what it
// wrote is still there, then the end; the wait closes the program's end
TEST_F(IoExecBounds_Tests, AChildEndsBeforeItsOutputIsRead) {
    io::command c("/bin/sh", "-c", "printf hello; printf oops >&2");
    auto out = c.stdout_pipe();
    ASSERT_TRUE(out);
    auto err = c.stderr_pipe();
    ASSERT_TRUE(err);
    ASSERT_TRUE(c.start());
    ASSERT_TRUE(ended(c.process.pid()));
    EXPECT_EQ(value_of(out->read_all_text()), "hello");
    EXPECT_EQ(value_of(err->read_all_text()), "oops");
    ASSERT_TRUE(c.wait());
    byte one[1];
    EXPECT_EQ(error_of(out->read(one)).code(), errc::closed);

    io::command quiet("/usr/bin/true");
    auto q = quiet.stdout_pipe();
    ASSERT_TRUE(q);
    ASSERT_TRUE(quiet.start());
    ASSERT_TRUE(ended(quiet.process.pid()));
    EXPECT_EQ(value_of(q->read_all_text()), "");   // nothing written: the end at once
    ASSERT_TRUE(quiet.wait());
}

// A child that has exited before the program writes its input: EPIPE, no
// signal; the wait succeeds. An input longer than the child reads, copied
// by a task, ends with the child, and the run succeeds
TEST_F(IoExecBounds_Tests, AChildEndsBeforeItsInputIsWritten) {
    io::command c("/usr/bin/true");
    auto in = c.stdin_pipe();
    ASSERT_TRUE(in);
    ASSERT_TRUE(c.start());
    ASSERT_TRUE(ended(c.process.pid()));
    std::string big(1 << 20, 'i');
    EXPECT_EQ(error_of(in->write(big)).code(), std::errc::broken_pipe);
    ASSERT_TRUE(c.wait());

    io::command head("/bin/sh", "-c", "head -c 1 >/dev/null");
    head.in = buffer(string(big.c_str()));
    EXPECT_TRUE(head.run());
    io::command none("/usr/bin/true");
    none.in = buffer(string(big.c_str()));
    EXPECT_TRUE(none.run());
}

// Calls out of order: a wait before the start, a second start, a second
// wait (the state of the first kept), a pipe asked for after the start or
// over a stream set, output with out set
TEST_F(IoExecBounds_Tests, CallsOutOfOrder) {
    io::command c("/bin/sh", "-c", "exit 3");
    EXPECT_EQ(error_of(c.wait()).code(), errc::process_done);
    EXPECT_EQ(error_of(spawn(c.async_wait()).wait()).code(), errc::process_done);
    EXPECT_FALSE(c.state);
    ASSERT_TRUE(c.start());
    EXPECT_EQ(error_of(c.start()).code(), errc::process_done);
    EXPECT_EQ(error_of(c.stdin_pipe()).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(c.stdout_pipe()).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(c.stderr_pipe()).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(c.wait()).code(), errc::exit_status);
    ASSERT_TRUE(c.state);
    EXPECT_EQ(c.state->exit_code(), 3);
    EXPECT_EQ(error_of(c.wait()).code(), errc::process_done);
    EXPECT_EQ(c.state->exit_code(), 3);
    EXPECT_EQ(error_of(c.run()).code(), errc::process_done);
    EXPECT_EQ(error_of(c.output()).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(c.process.wait()).code(), errc::process_done);
    EXPECT_EQ(error_of(c.process.signal(SIGTERM)).code(), errc::process_done);
    EXPECT_EQ(error_of(c.process.kill()).code(), errc::process_done);
    EXPECT_EQ(error_of(c.process.release()).code(), errc::process_done);

    io::command set("/bin/echo");
    buffer b;
    set.out = b;
    EXPECT_EQ(error_of(set.stdout_pipe()).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(set.output()).code(), std::errc::invalid_argument);
    EXPECT_EQ(error_of(set.combined_output()).code(), std::errc::invalid_argument);
    set.err = b;
    EXPECT_EQ(error_of(set.stderr_pipe()).code(), std::errc::invalid_argument);
    set.in = b;
    EXPECT_EQ(error_of(set.stdin_pipe()).code(), std::errc::invalid_argument);

    io::command released("/bin/sh", "-c", "exit 0");
    ASSERT_TRUE(released.start());
    ASSERT_TRUE(released.process.release());
    EXPECT_EQ(error_of(released.process.release()).code(), errc::process_done);
    EXPECT_EQ(error_of(released.process.wait()).code(), errc::process_done);
    EXPECT_EQ(error_of(released.wait()).code(), errc::process_done);
    ::waitpid(released.process.pid(), nullptr, 0);   // the child let go of: reaped here
}

// Names that are no program: empty, a directory, a path with a trailing
// separator, a file that is not executable, a missing working directory;
// the command stays unstarted
TEST_F(IoExecBounds_Tests, NamesThatAreNoProgram) {
    EXPECT_EQ(error_of(look_path("")).code(), errc::not_found);
    EXPECT_EQ(error_of(look_path("/bin/")).code(), errc::not_found);
    EXPECT_EQ(error_of(look_path("/bin/sh/")).code(), errc::not_found);
    ASSERT_TRUE(write_file(at("script"), "#!/bin/sh\n"));
    EXPECT_EQ(error_of(look_path(at("script"))).code(), errc::not_found);   // not executable
    io::command empty("");
    EXPECT_EQ(error_of(empty.start()).code(), errc::not_found);
    EXPECT_FALSE(empty.process);
    io::command dir{string(_dir)};
    EXPECT_EQ(error_of(dir.run()).code(), errc::not_found);
    io::command nowhere("/bin/echo");
    nowhere.dir = at("missing");
    EXPECT_FALSE(nowhere.start());
    EXPECT_FALSE(nowhere.process);
}

// Arguments and environment at their bounds: an empty argument is passed,
// an empty environment is empty, an argv0 given is the child's
TEST_F(IoExecBounds_Tests, EmptyArgumentsAndEnvironment) {
    io::command blank("/bin/echo", "");
    EXPECT_EQ(value_of(blank.output()), "\n");
    io::command count("/bin/sh", "-c", "echo $#", "zero", "", "");
    EXPECT_EQ(value_of(count.output()), "2\n");
    io::command env("/usr/bin/env");
    env.env = vector<pair<string, string>>();
    EXPECT_EQ(value_of(env.output()), "");
    io::command name("/bin/sh", "-c", "echo $0");
    name.argv0 = string("");
    EXPECT_EQ(value_of(name.output()), "\n");
}

// A writer that fails while the child's output is copied into it: the wait
// reports its error, as Go's Wait reports the copy's, after the exit status
TEST_F(IoExecBounds_Tests, AWriterFailsWhileTheOutputIsCopied) {
    refusing r;
    io::command c("/bin/echo", "lost");
    c.out = r;
    EXPECT_EQ(error_of(c.run()).code(), std::errc::no_space_on_device);
    EXPECT_GT(r.asked, 0u);
    ASSERT_TRUE(c.state);
    EXPECT_TRUE(c.state->success());

    refusing r2;
    io::command failing("/bin/sh", "-c", "echo lost; exit 2");
    failing.out = r2;
    EXPECT_EQ(error_of(failing.run()).code(), errc::exit_status);   // the exit status first

    auto closed = create(at("closed"));
    ASSERT_TRUE(closed);
    ASSERT_TRUE(closed->close());
    io::command into_closed("/bin/echo", "lost");
    into_closed.out = *closed;
    EXPECT_EQ(error_of(into_closed.run()).code(), errc::closed);

    io::command from_closed("/bin/cat");
    from_closed.in = *closed;
    EXPECT_EQ(error_of(from_closed.output()).code(), errc::closed);
}

// The defaults: a process that holds none, a state of no process
TEST_F(IoExecBounds_Tests, TheDefaults) {
    io::process p;
    EXPECT_FALSE(p);
    EXPECT_TRUE(p == io::process());
    process_state s;
    EXPECT_EQ(s.pid(), 0);
    EXPECT_TRUE(s.exited());
    EXPECT_EQ(s.exit_code(), 0);
    EXPECT_TRUE(s.success());
    EXPECT_FALSE(s.signaled());
    EXPECT_EQ(s.signal(), 0);
    EXPECT_EQ(s.user_time().count(), 0);
    EXPECT_EQ(s.to_string(), "exit status 0");
    io::command c("/bin/sh", "-c", "kill -9 $$");
    EXPECT_EQ(error_of(c.run()).code(), errc::exit_status);
    ASSERT_TRUE(c.state);
    EXPECT_EQ(c.state->exit_code(), -1);
    EXPECT_FALSE(c.state->exited());
    EXPECT_EQ(c.state->signal(), SIGKILL);
    EXPECT_EQ(c.state->to_string(), "signal: killed");
    io::command moved("/bin/echo", "m");
    io::command into = std::move(moved);
    EXPECT_EQ(value_of(into.output()), "m\n");
}
