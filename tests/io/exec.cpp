//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// exec: command, process, process_state, look_path — a child made with
// posix_spawn, its streams as the library's, its end waited for on the
// reactor.
#include "tests/types.h"

using namespace sgcl::async;

#include <chrono>
#include <csignal>
#include <string>
#include <thread>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;
    using namespace std::chrono_literals;

    struct IoExec_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            auto d = make_temp_dir({}, "sgcl-exec-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = d->str();
        }

        void TearDown() override {
            io::remove_all(dir());
        }

        string dir() const {
            return string(_dir);
        }

        string at(const string& name) const {
            return path::join(dir(), name);
        }
    };
}

TEST_F(IoExec_Tests, LookPath) {
    auto sh = look_path("sh");
    ASSERT_TRUE(sh) << sh.error().message();
    EXPECT_TRUE(sh->starts_with("/") && sh->ends_with("/sh"));
    EXPECT_EQ(look_path(*sh).value(), *sh);                       // a path with a separator: itself, when executable
    auto none = look_path("no-such-program-sgcl");
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), errc::not_found);
    EXPECT_TRUE(none.error().is_not_found());                       // the predicate answers the module's own code too
    EXPECT_FALSE(look_path("/no/such/dir/sh"));
    EXPECT_FALSE(look_path(dir()));                                 // a directory is not executable
}

TEST_F(IoExec_Tests, RunOutputAndTheExitStatus) {
    io::command echo("echo", "hello", "world");
    auto out = echo.output();
    ASSERT_TRUE(out) << out.error().message();
    EXPECT_EQ(*out, "hello world\n");
    EXPECT_TRUE(echo.path.starts_with("/") && echo.path.ends_with("/echo"));   // found in PATH
    ASSERT_TRUE(echo.state);
    EXPECT_TRUE(echo.state->success() && echo.state->exited() && echo.state->exit_code() == 0 && !echo.state->signaled());
    EXPECT_EQ(echo.state->to_string(), "exit status 0");
    EXPECT_EQ(echo.state->pid(), echo.process->pid());

    // A failure status is an error, the code in the state, the standard
    // error captured for the message
    io::command failing("sh", "-c", "echo oops >&2; exit 3");
    auto r = failing.output();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_exit_status());
    EXPECT_EQ(r.error().code(), errc::exit_status);
    ASSERT_TRUE(failing.state);
    EXPECT_EQ(failing.state->exit_code(), 3);
    EXPECT_EQ(failing.state->to_string(), "exit status 3");
    EXPECT_EQ(failing.captured_err, "oops\n");

    // run: no capture, the null device for every stream
    io::command t("true");
    EXPECT_TRUE(t.run());
    io::command f("false");
    auto fr = f.run();
    ASSERT_FALSE(fr);
    EXPECT_EQ(f.state->exit_code(), 1);

    // A program that is not there fails at start
    io::command none("no-such-program-sgcl");
    auto nr = none.run();
    ASSERT_FALSE(nr);
    EXPECT_EQ(nr.error().code(), errc::not_found);
    EXPECT_FALSE(none.process);

    // Once
    EXPECT_FALSE(t.start());
    EXPECT_EQ(t.wait().error().code(), errc::process_done);
}

TEST_F(IoExec_Tests, TheArgumentsTheDirectoryAndTheEnvironment) {
    io::command args("sh", "-c", "printf '%s|' \"$0\" \"$@\"", "zero", "one two", "three");
    auto out = args.output();
    ASSERT_TRUE(out) << out.error().message();
    EXPECT_EQ(*out, "zero|one two|three|");

    io::command from_vector("printf", vector<string>{"%s-%s", "a", "b"});
    EXPECT_EQ(from_vector.output().value(), "a-b");

    io::command argv0("sh", "-c", "echo $0");
    argv0.argv0 = "renamed";
    EXPECT_EQ(argv0.output().value(), "renamed\n");

    io::command pwd("pwd");
    pwd.dir = dir();
    auto where = pwd.output();
    ASSERT_TRUE(where) << where.error().message();
    EXPECT_TRUE(where->trim() == dir() || where->trim().ends_with(path::base(dir())));   // /private/tmp against /tmp on macOS

    io::command bad_dir("pwd");
    bad_dir.dir = at("missing");
    EXPECT_FALSE(bad_dir.run());

    io::command env("sh", "-c", "echo \"$SGCL_EXEC_TEST-$HOME\"");
    env.env = vector<pair<string, string>>{{"SGCL_EXEC_TEST", "set"}};   // the whole environment: HOME is gone
    EXPECT_EQ(env.output().value(), "set-\n");
    io::command inherited("sh", "-c", "echo \"$SGCL_EXEC_TEST\"");
    ::setenv("SGCL_EXEC_TEST", "inherited", 1);
    EXPECT_EQ(inherited.output().value(), "inherited\n");
    ::unsetenv("SGCL_EXEC_TEST");
}

TEST_F(IoExec_Tests, TheStreamsAsFilesBuffersAndPipes) {
    // A buffer as the input: copied through a pipe by a task; a buffer as
    // the output: the pipe drained into it
    io::command tr("tr", "a-z", "A-Z");
    tracked_ptr<buffer> input = make_tracked<buffer>(string("quiet please\n"));
    tracked_ptr<buffer> output = make_tracked<buffer>();
    tr.in = input;
    tr.out = output;
    ASSERT_TRUE(tr.run()) << "tr";
    EXPECT_EQ(output->text(), "QUIET PLEASE\n");

    // A file as the output: the descriptor inherited, no task
    auto log = create(at("log"));
    ASSERT_TRUE(log);
    io::command to_file("sh", "-c", "echo to a file; echo and an error >&2");
    to_file.out = *log;
    to_file.err = *log;                          // the same file for both: one descriptor
    ASSERT_TRUE(to_file.run());
    (void)(*log)->close();
    EXPECT_EQ(read_text(at("log")).value(), "to a file\nand an error\n");

    // A file as the input
    ASSERT_TRUE(write_file(at("in"), string("3\n1\n2\n")));
    auto in = open(at("in"));
    ASSERT_TRUE(in);
    io::command sort("sort");
    sort.in = *in;
    EXPECT_EQ(sort.output().value(), "1\n2\n3\n");

    // combined_output: one pipe for both, in the order written
    io::command both("sh", "-c", "echo one; echo two >&2; echo three");
    EXPECT_EQ(both.combined_output().value(), "one\ntwo\nthree\n");

    // The same buffer in out and err: one pipe too
    io::command same("sh", "-c", "echo x; echo y >&2");
    tracked_ptr<buffer> joined = make_tracked<buffer>();
    same.out = joined;
    same.err = joined;
    ASSERT_TRUE(same.run());
    EXPECT_EQ(joined->text(), "x\ny\n");

    // More than a pipe holds, both ways: the copying tasks keep the child
    // from blocking on a full pipe
    std::string big(1 << 20, 'z');
    io::command cat("cat");
    cat.in = make_tracked<buffer>(string(big));
    auto echoed = cat.output();
    ASSERT_TRUE(echoed) << echoed.error().message();
    EXPECT_EQ(echoed->size(), big.size());
}

TEST_F(IoExec_Tests, ThePipesOfTheProgramsOwn) {
    // stdin_pipe: the program writes, the child reads
    io::command wc("wc", "-c");
    auto in = wc.stdin_pipe();
    ASSERT_TRUE(in) << in.error().message();
    tracked_ptr<buffer> out = make_tracked<buffer>();
    wc.out = out;
    ASSERT_TRUE(wc.start());
    ASSERT_TRUE((*in)->write("twelve chars"));
    ASSERT_TRUE((*in)->close());                    // the end of the input
    ASSERT_TRUE(wc.wait());
    EXPECT_EQ(out->text().trim(), "12");

    // stdout_pipe: the child writes, the program reads it to the end, then waits
    io::command seq("sh", "-c", "for i in 1 2 3; do echo line $i; done");
    auto from = seq.stdout_pipe();
    ASSERT_TRUE(from);
    ASSERT_TRUE(seq.start());
    tracked_ptr<buffered_reader> lines = make_tracked<buffered_reader>(*from);
    size_t n = 0;
    for (auto line : lines->lines()) {
        EXPECT_TRUE(line.starts_with("line "));
        ++n;
    }
    EXPECT_EQ(n, 3u);
    ASSERT_TRUE(seq.wait());
    EXPECT_TRUE((*from)->is_closed());              // wait closed it

    // stderr_pipe
    io::command err("sh", "-c", "echo to stderr >&2");
    auto e = err.stderr_pipe();
    ASSERT_TRUE(e);
    ASSERT_TRUE(err.start());
    auto text = (*e)->read_all_text();
    ASSERT_TRUE(text);
    EXPECT_EQ(*text, "to stderr\n");
    ASSERT_TRUE(err.wait());

    // A pipe asked for twice, or after the stream was set, is refused
    io::command twice("true");
    twice.in = make_tracked<buffer>();
    EXPECT_FALSE(twice.stdin_pipe());
}

TEST_F(IoExec_Tests, TheProcessItsSignalsAndTheStop) {
    io::command sleeping("sleep", "30");
    ASSERT_TRUE(sleeping.start());
    ASSERT_TRUE(sleeping.process);
    EXPECT_GT(sleeping.process->pid(), 0);
    EXPECT_TRUE(sleeping.process->signal(SIGTERM));
    auto r = sleeping.wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_exit_status());
    ASSERT_TRUE(sleeping.state);
    EXPECT_TRUE(sleeping.state->signaled() && !sleeping.state->exited());
    EXPECT_EQ(sleeping.state->signal(), SIGTERM);
    EXPECT_EQ(sleeping.state->exit_code(), -1);
    EXPECT_TRUE(sleeping.state->to_string().starts_with("signal: "));
    EXPECT_EQ(sleeping.process->kill().error().code(), errc::process_done);   // waited for: no signal to it

    // kill
    io::command killed("sleep", "30");
    ASSERT_TRUE(killed.start());
    EXPECT_TRUE(killed.process->kill());
    EXPECT_FALSE(killed.wait());
    EXPECT_EQ(killed.state->signal(), SIGKILL);

    // The stop token: the child killed when the stop is requested
    stop_source source;
    io::command stopped("sleep", "30");
    stopped.stop = source.token();
    ASSERT_TRUE(stopped.start());
    auto started = clock::now();
    std::thread requester([&] {
        std::this_thread::sleep_for(50ms);
        source.request_stop();
    });
    auto sr = stopped.wait();
    requester.join();
    EXPECT_FALSE(sr);
    EXPECT_EQ(stopped.state->signal(), SIGKILL);
    EXPECT_LT(clock::now() - started, 5s);

    // A stop requested before the start: killed at once
    stop_source early;
    early.request_stop();
    io::command never("sleep", "30");
    never.stop = early.token();
    ASSERT_TRUE(never.start());
    EXPECT_FALSE(never.wait());
    EXPECT_EQ(never.state->signal(), SIGKILL);

    // release: nothing more from the object
    io::command released("true");
    ASSERT_TRUE(released.start());
    EXPECT_TRUE(released.process->release());
    EXPECT_EQ(released.process->wait().error().code(), errc::process_done);
    (void)::waitpid(released.process->pid(), nullptr, 0);   // reaped by hand, as release leaves it to the program

    // A process group of its own: the shell and its child both end on
    // a signal to the group
    io::command group("sh", "-c", "sleep 30; echo never");
    group.set_pgid = true;
    ASSERT_TRUE(group.start());
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(::kill(-group.process->pid(), SIGTERM), 0);
    EXPECT_FALSE(group.wait());
    EXPECT_EQ(group.state->signal(), SIGTERM);
}

TEST_F(IoExec_Tests, TheWaitDelay) {
    // A child that ends but leaves a grandchild holding its stdout: with
    // no delay the wait would hang on the pipe; with a delay it ends the
    // copying, reports the delay and closes the pipe
    io::command leaving("sh", "-c", "sleep 2 & echo started");
    leaving.wait_delay = 200ms;
    tracked_ptr<buffer> out = make_tracked<buffer>();
    leaving.out = out;
    auto started = clock::now();
    auto r = leaving.run();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::wait_delay);
    EXPECT_TRUE(leaving.state && leaving.state->success());   // the shell itself exited 0
    EXPECT_LT(clock::now() - started, 5s);
    EXPECT_EQ(out->text(), "started\n");
}

namespace {
    task<string> async_case() {
        io::command echo("echo", "from a task");
        auto out = co_await echo.async_output();
        if (!out) {
            co_return out.error().message();
        }
        io::command tr("tr", "a-z", "A-Z");
        tr.in = make_tracked<buffer>(string("piped\n"));
        auto upper = co_await tr.async_output();
        if (!upper) {
            co_return upper.error().message();
        }
        io::command failing("sh", "-c", "exit 7");
        auto fr = co_await failing.async_run();
        io::command sleeping("sleep", "30");
        if (!sleeping.start()) {
            co_return "start";
        }
        (void)sleeping.process->signal(SIGTERM);
        auto sr = co_await sleeping.async_wait();
        co_return out->trim() + "|" + upper->trim() + "|" + to_string(failing.state->exit_code()) + "|" + (fr ? "ok" : "err") + "|" + to_string(sleeping.state->signal());
    }

    task<size_t> many_at_once(int n) {
        vector<task<expected<string, io::error>>> outputs;
        vector<io::command> commands;
        for (int i : range(n)) {
            commands.push_back(io::command("sh", "-c", "sleep 0.05; echo " + to_string(i)));
        }
        for (auto& c : commands) {
            outputs.push_back(spawn(c.async_output()));
        }
        size_t ok = 0;
        for (int i : range(n)) {
            auto r = co_await outputs[i];
            ok += r && r->trim() == to_string(i);
        }
        co_return ok;
    }
}

TEST_F(IoExec_Tests, FromATask) {
    auto t = sgcl::async::spawn(async_case());
    EXPECT_EQ(t.wait(), "from a task|PIPED|7|err|15");

    // Twenty children at once, waited for on the reactor: a burst of
    // 50 ms sleeps ends in well under twenty times that
    auto started = clock::now();
    auto ok = sgcl::async::spawn(many_at_once(20));
    EXPECT_EQ(ok.wait(), 20u);
    EXPECT_LT(clock::now() - started, 2s);
}
