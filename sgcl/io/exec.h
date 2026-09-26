//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "file.h"
#include "os.h"
#include "path.h"
#include "stream.h"
#include "../async/coroutine.h"
#include "../async/reactor.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timeout.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/string.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace sgcl::io {
    // Running a program: what Go's os/exec has, in the shapes of the
    // library. A `command` is a value — the program, its arguments and
    // what it is given (a directory, an environment, three streams),
    // set by name in any order after the constructor, as the fields of
    // exec.Cmd — that start() runs and wait() collects; run() is the
    // two, output() the two with the standard output captured. The
    // program's streams are the library's: a `file` in `in`, `out` or
    // `err` is inherited as a descriptor, any other reader or writer is
    // served through a pipe by a task that copies (Go's goroutines), a
    // null pointer is the null device. A child started is a `process`
    // (Go's os.Process): its id, a signal, a wait; a child ended is a
    // `process_state`: its exit code, its signal, its times.
    //
    // The child is made with posix_spawn, never fork: a fork of a
    // process with the collector's threads would hold their locks in a
    // child that has none of them. Its exit is waited for on the
    // reactor (exited(pid): kqueue's EVFILT_PROC here, a pidfd on Linux),
    // so wait() holds no thread while the child runs, as Go 1.23
    // waits on Linux; wait() from a thread is waitpid.
    class process;
    class command;

    // What a process ended as: Go's os.ProcessState, a value made by wait()
    class process_state {
    public:
        process_state() noexcept = default;

        // Copied field by field, not as bytes: a trivially copyable value
        // would make optional<process_state> trivially copyable, and a
        // command moved off a stack would carry the stack's garbage in
        // the storage of its empty `state` into a managed object, where
        // the collector's debug check would take a stale word for a
        // pointer
        process_state(const process_state& o) noexcept
        : _pid(o._pid)
        , _status(o._status)
        , _user(o._user)
        , _system(o._system) {
        }

        process_state& operator=(const process_state& o) noexcept {
            _pid = o._pid;
            _status = o._status;
            _user = o._user;
            _system = o._system;
            return *this;
        }

        // The process id
        int pid() const noexcept {
            return _pid;
        }

        // Whether it ended by exiting (not by a signal)
        bool exited() const noexcept {
            return WIFEXITED(_status);
        }

        // The exit code; -1 when it did not exit (ended by a signal)
        int exit_code() const noexcept {
            return exited() ? WEXITSTATUS(_status) : -1;
        }

        // Exited with 0
        bool success() const noexcept {
            return exited() && WEXITSTATUS(_status) == 0;
        }

        // Ended by a signal, and which
        bool signaled() const noexcept {
            return WIFSIGNALED(_status);
        }

        int signal() const noexcept {
            return signaled() ? WTERMSIG(_status) : 0;
        }

        // The CPU time of the process in user and in kernel mode
        std::chrono::microseconds user_time() const noexcept {
            return _user;
        }

        std::chrono::microseconds system_time() const noexcept {
            return _system;
        }

        // "exit status 1", "signal: killed": what Go's String says
        string to_string() const {
            if (exited()) {
                return string("exit status ") + sgcl::to_string(exit_code());
            }
            if (signaled()) {
                std::string s = "signal: ";
                const char* name = ::strsignal(signal());
                s += name ? name : "unknown";
                return string(s);
            }
            return "";
        }

    private:
        friend class process;

        process_state(int pid, int status, const rusage& usage) noexcept
        : _pid(pid)
        , _status(status)
        , _user(std::chrono::seconds(usage.ru_utime.tv_sec) + std::chrono::microseconds(usage.ru_utime.tv_usec))
        , _system(std::chrono::seconds(usage.ru_stime.tv_sec) + std::chrono::microseconds(usage.ru_stime.tv_usec)) {
        }

        int _pid = 0;
        int _status = 0;
        std::chrono::microseconds _user{};
        std::chrono::microseconds _system{};
    };

    // A running child: Go's os.Process. Made by command::start(); waited
    // for once, from a thread or a task; signalled while it runs. Its
    // destructor waits for nothing (a wait in a destructor blocks the
    // sweep) and releases nothing: a child never waited for stays a
    // zombie until the program ends, as everywhere.
    class process final {
        friend class sgcl::detail::MakerBase;   // make_tracked constructs a process, in command::start
        friend class command;                   // its wait waits for the process as a thread does

        explicit process(int pid) noexcept
        : _pid(pid) {
        }

    public:
        int pid() const noexcept {
            return _pid;
        }

        // A signal to the process: errc::process_done once it was waited
        // for or released (its id may be another process's by then); a
        // signal while a wait is in progress is what ends the wait
        expected<void, error> signal(int sig) {
            if (_done.load(std::memory_order_acquire)) {
                return detail::fail(error(errc::process_done, "signal"));
            }
            if (::kill(_pid, sig) != 0) {
                return detail::fail(last_error("signal"));
            }
            return {};
        }

        // SIGKILL: the process ends, now
        expected<void, error> kill() {
            return signal(SIGKILL);
        }

        // The process's end and how it ended (waitpid); a second wait, or
        // one after release(), is errc::process_done. `p.wait()` on this
        // thread, `co_await p.async_wait()` in a task
        expected<process_state, error> wait() {
            return _block_wait();
        }

        async::task<expected<process_state, error>> async_wait() {
            return _co_wait();
        }

        // The process let go of without a wait: its resources are the
        // system's once it ends, and this object answers nothing more
        expected<void, error> release() {
            if (_waited.exchange(true, std::memory_order_acq_rel)) {
                return detail::fail(error(errc::process_done, "release"));
            }
            _done.store(true, std::memory_order_release);
            return {};
        }

    private:
        expected<process_state, error> _reap(int options) {
            for (;;) {
                int status = 0;
                rusage usage{};
                int r = ::wait4(_pid, &status, options, &usage);
                if (r == _pid) {
                    return process_state(_pid, status, usage);
                }
                if (r < 0 && errno == EINTR) {
                    continue;
                }
                return detail::fail(last_error("wait"));
            }
        }

        int _pid;
        std::atomic<bool> _waited{false};   // a wait or a release begun: no second one
        std::atomic<bool> _done{false};     // the wait or the release complete: the id is no longer this process's

        // the two halves of the operations above: a thread's and a task's
        expected<process_state, error> _block_wait()  {
            if (_waited.exchange(true, std::memory_order_acq_rel)) {
                return detail::fail(error(errc::process_done, "wait"));
            }
            auto r = _reap(0);
            _done.store(true, std::memory_order_release);
            return r;
        }

        // The same from a task: the end waited for on the reactor, no
        // thread held meanwhile, the status collected once it came
        async::task<expected<process_state, error>> _co_wait()  {
            if (_waited.exchange(true, std::memory_order_acq_rel)) {
                co_return detail::fail(error(errc::process_done, "wait"));
            }
            co_await async::exited(_pid)->receive();
            auto r = _reap(0);
            _done.store(true, std::memory_order_release);
            co_return r;
        }
    };

    // The executable a name stands for: the name itself when it holds a
    // separator and names an executable file, else the first executable
    // file of that name in the directories of PATH (an empty entry is the
    // current directory); errc::not_found for none. Go's exec.LookPath.
    inline expected<string, error> look_path(const string& file) {
        auto executable = [](const std::string& p) {
            struct stat st;
            return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode) && ::access(p.c_str(), X_OK) == 0;
        };
        if (file.contains('/')) {
            if (executable(file.str())) {
                return file;
            }
            return detail::fail(error(errc::not_found, "look_path", file));
        }
        if (file.empty()) {
            return detail::fail(error(errc::not_found, "look_path", file));
        }
        const char* env = ::getenv("PATH");
        std::string paths = env ? env : "";
        size_t start = 0;
        for (;;) {
            size_t colon = paths.find(':', start);
            std::string dir = paths.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
            if (dir.empty()) {
                dir = ".";
            }
            std::string candidate = dir + '/' + file.str();
            if (executable(candidate)) {
                return string(candidate);
            }
            if (colon == std::string::npos) {
                break;
            }
            start = colon + 1;
        }
        return detail::fail(error(errc::not_found, "look_path", file));
    }

    // A program to run: Go's exec.Cmd. The constructor takes the name
    // and the arguments (exec.Command); the rest is set by name, in any
    // order, before start(). A value: on a stack or inside a managed
    // object (it holds the streams and the process through tracked
    // pointers); moved, not copied, since it owns the tasks that serve
    // its pipes. What start() sets stays for reading: process, and
    // after wait() state.
    class command {
    public:
        // The program and its arguments, not counting argv[0], which is
        // the name as given (argv0 changes it). A name without a
        // separator is looked up in PATH by start() (look_path), and path
        // is the result then.
        template<class... Args>
        requires (std::is_convertible_v<const Args&, string> && ...)
        explicit command(const string& name, const Args&... arguments)
        : path(name)
        , args{string(arguments)...} {
        }

        command(const string& name, vector<string> arguments)
        : path(name)
        , args(std::move(arguments)) {
        }

        command(command&&) noexcept = default;
        command& operator=(command&&) noexcept = default;

        string path;                               // the program: as given, then what look_path found
        vector<string> args;                       // the arguments after the program's name
        optional<string> argv0;                    // argv[0] when not the name as given
        optional<vector<pair<string, string>>> env;   // the environment: nullopt is the program's own, inherited; a list is the child's whole environment
        string dir;                                // the working directory; empty is the program's own
        io::reader in;                    // standard input: a file inherited as a descriptor, another reader copied through a pipe by a task, null the null device
        io::writer out;                   // standard output: the same; the same object in out and err gets one pipe
        io::writer err;                   // standard error
        bool set_pgid = false;                     // the child a process group of its own (Setpgid): a signal to -pid reaches it and its children
        async::stop_token stop;                           // the child killed when the stop is requested (Go's CommandContext with the default Cancel)
        duration wait_delay = duration::zero();    // wait() gives the copying tasks this long after the child ended (or the stop came) before closing the pipes; zero waits for them (Go's WaitDelay)

        tracked_ptr<io::process> process;          // the child, once started
        optional<process_state> state;             // how it ended, once waited for
        string captured_err;                       // output(): what the child wrote to standard error when err was null (Go's ExitError.Stderr)

        // The child started: the program found, the streams arranged, the
        // process made with posix_spawn. Once: a second start is an error.
        expected<void, error> start() {
            if (process) {
                return detail::fail(error(errc::process_done, "start", path));
            }
            if (!path.contains('/')) {
                auto found = look_path(path);
                if (!found) {
                    return detail::fail(found);
                }
                path = *found;
            } else if (auto found = look_path(path); !found) {
                return detail::fail(found);
            }
            Spawn spawn(*this);
            auto arranged = spawn.arrange();
            if (!arranged) {
                return detail::fail(arranged);
            }
            int pid = 0;
            int rc = ::posix_spawn(&pid, path.c_str(), &spawn.actions, &spawn.attr, spawn.argv.data(), spawn.envp.data());
            if (rc != 0) {
                errno = rc;
                return detail::fail(last_error("start", path));
            }
            spawn.close_child_ends();
            for (auto& f : _child_ends) {   // the child's ends of stdin_pipe and the others: the child has them, this side lets go
                (void)f->close();
            }
            _child_ends.clear();
            process = make_tracked<io::process>(pid);
            _done = make_tracked<async::channel<void>>();
            for (auto& c : spawn.copies) {   // the copying tasks run from now, as Go's goroutines do
                _copies.push_back(async::spawn(std::move(c)));
            }
            for (auto& f : spawn.parent_ends) {
                _pipes.push_back(f);
            }
            if (stop.stop_possible()) {
                async::go(_watch_stop(process, stop, _done));
            }
            return {};
        }

        // The child waited for, on the calling thread: its state kept in
        // `state`, the copying tasks joined (within wait_delay when set),
        // the pipes closed. A child that ended with a failure status is
        // errc::exit_status (the code in state), as Go's ExitError.
        // `c.wait()` on this thread, `co_await c.async_wait()` in a task
        expected<void, error> wait() {
            return _block_wait();
        }

        async::task<expected<void, error>> async_wait() {
            return _co_wait();
        }

        // start() and wait()
        // `run(...)` on this thread, `co_await async_run(...)` in a task
        expected<void, error> run() {
            return _block_run();
        }

        async::task<expected<void, error>> async_run() {
            return _co_run();
        }

        // run() with the standard output captured and returned; when err
        // is null, the standard error is captured too, into captured_err,
        // for the message of a failure. out must be null.
        // `output(...)` on this thread, `co_await async_output(...)` in a task
        expected<string, error> output() {
            return _block_output();
        }

        async::task<expected<string, error>> async_output() {
            return _co_output();
        }

        // run() with the standard output and error captured together, in
        // the order the child wrote them. out and err must be null.
        // `combined_output(...)` on this thread, `co_await async_combined_output(...)` in a task
        expected<string, error> combined_output() {
            return _block_combined_output();
        }

        async::task<expected<string, error>> async_combined_output() {
            return _co_combined_output();
        }

        // A pipe to the child's standard input (output, error), the
        // program's end returned, before start(): what is written there
        // the child reads. wait() closes the program's end of an input
        // pipe once the child ended; the output pipes are read to their
        // end before wait() (a wait first may block the child on a full
        // pipe). Go's StdinPipe, StdoutPipe, StderrPipe.
        expected<tracked_ptr<file>, error> stdin_pipe() {
            if (in || process) {
                return detail::fail(error(std::make_error_code(std::errc::invalid_argument), "stdin_pipe", path));
            }
            auto p = pipe();
            if (!p) {
                return detail::fail(p);
            }
            _child_end(*p->read);
            _child_ends.push_back(p->read);
            in = p->read;
            _pipes.push_back(p->write);
            return p->write;
        }

        expected<tracked_ptr<file>, error> stdout_pipe() {
            if (out || process) {
                return detail::fail(error(std::make_error_code(std::errc::invalid_argument), "stdout_pipe", path));
            }
            auto p = pipe();
            if (!p) {
                return detail::fail(p);
            }
            _child_end(*p->write);
            _child_ends.push_back(p->write);
            out = p->write;
            _pipes.push_back(p->read);
            return p->read;
        }

        expected<tracked_ptr<file>, error> stderr_pipe() {
            if (err || process) {
                return detail::fail(error(std::make_error_code(std::errc::invalid_argument), "stderr_pipe", path));
            }
            auto p = pipe();
            if (!p) {
                return detail::fail(p);
            }
            _child_end(*p->write);
            _child_ends.push_back(p->write);
            err = p->write;
            _pipes.push_back(p->read);
            return p->read;
        }

    private:
        // What start() builds for posix_spawn: the argument and
        // environment vectors, the file actions (the three descriptors,
        // the directory), the attributes, and the pipes with their tasks
        friend struct Spawn;

        struct Spawn {
            explicit Spawn(command& c)
            : cmd(c) {
                ::posix_spawn_file_actions_init(&actions);
                ::posix_spawnattr_init(&attr);
            }

            ~Spawn() {
                ::posix_spawn_file_actions_destroy(&actions);
                ::posix_spawnattr_destroy(&attr);
                if (null_fd >= 0) {
                    ::close(null_fd);
                }
                close_child_ends();
            }

            expected<void, error> arrange() {
                argv_storage.push_back(cmd.argv0 ? cmd.argv0->str() : cmd.path.str());
                for (auto& a : cmd.args) {
                    argv_storage.push_back(a.str());
                }
                for (auto& a : argv_storage) {
                    argv.push_back(a.data());
                }
                argv.push_back(nullptr);
                if (cmd.env) {
                    for (auto& [k, v] : *cmd.env) {
                        env_storage.push_back(k.str() + '=' + v.str());
                    }
                    for (auto& e : env_storage) {
                        envp.push_back(e.data());
                    }
                    envp.push_back(nullptr);
                } else {
                    for (char** e = ::environ; *e; ++e) {
                        envp.push_back(*e);
                    }
                    envp.push_back(nullptr);
                }
                short flags = 0;
                if (cmd.set_pgid) {
                    flags |= POSIX_SPAWN_SETPGROUP;
                    ::posix_spawnattr_setpgroup(&attr, 0);
                }
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
                flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;   // every descriptor but the three arranged is closed in the child (Apple)
#endif
                ::posix_spawnattr_setflags(&attr, flags);
                if (!cmd.dir.empty()) {
                    int rc = _addchdir(cmd.dir.c_str());
                    if (rc != 0) {
                        errno = rc;
                        return detail::fail(last_error("start", cmd.dir));
                    }
                }
                if (auto r = _input(cmd.in, 0); !r) {
                    return r;
                }
                if (cmd.out && cmd.out == cmd.err) {   // one writer for both: one pipe, or one descriptor
                    if (auto r = _output(cmd.out, 1); !r) {
                        return r;
                    }
                    ::posix_spawn_file_actions_adddup2(&actions, 1, 2);
                    return {};
                }
                if (auto r = _output(cmd.out, 1); !r) {
                    return r;
                }
                return _output(cmd.err, 2);
            }

            void close_child_ends() {
                for (auto& f : child_ends) {
                    (void)f->close();
                }
                child_ends.clear();
            }

            command& cmd;
            posix_spawn_file_actions_t actions;
            posix_spawnattr_t attr;
            std::vector<std::string> argv_storage;
            std::vector<std::string> env_storage;
            std::vector<char*> argv;
            std::vector<char*> envp;
            int null_fd = -1;
            vector<tracked_ptr<file>> child_ends;       // the child's ends of the pipes, closed here after the spawn
            vector<tracked_ptr<file>> parent_ends;      // the program's ends, closed by wait
            vector<async::task<void>> copies;                  // the tasks copying between a stream and a pipe

        private:
            int _null() {
                if (null_fd < 0) {
                    null_fd = ::open("/dev/null", O_RDWR | O_CLOEXEC);
                }
                return null_fd;
            }

            // The child's descriptor `target` from a reader: the null
            // device, a file's own descriptor, or the read end of a pipe
            // whose write end a task feeds from the reader
            expected<void, error> _input(const io::reader& r, int target) {
                if (!r) {
                    return _dup(_null(), target);
                }
                if (int fd = r.fd(); fd >= 0) {   // a file, a standard stream: its descriptor as it is
                    return _dup(fd, target);
                }
                auto p = pipe();
                if (!p) {
                    return detail::fail(p);
                }
                command::_child_end(*p->read);
                if (auto d = _dup(p->read->fd(), target); !d) {
                    return d;
                }
                child_ends.push_back(p->read);
                parent_ends.push_back(p->write);
                copies.push_back(_copy_in(r, p->write));
                return {};
            }

            // The same from a writer: the write end of a pipe whose read
            // end a task drains into the writer
            expected<void, error> _output(const io::writer& w, int target) {
                if (!w) {
                    return _dup(_null(), target);
                }
                if (int fd = w.fd(); fd >= 0) {
                    return _dup(fd, target);
                }
                auto p = pipe();
                if (!p) {
                    return detail::fail(p);
                }
                command::_child_end(*p->write);
                if (auto d = _dup(p->write->fd(), target); !d) {
                    return d;
                }
                child_ends.push_back(p->write);
                parent_ends.push_back(p->read);
                copies.push_back(_copy_out(w, p->read));
                return {};
            }

            // The child's descriptor `target` made from fd: a dup2 in the
            // child, which also lifts the close-on-exec flag every
            // descriptor of the library carries; a descriptor that is
            // its own target (the program's stdin as the child's) is
            // marked inherited instead
            // The child's working directory: the POSIX-2024 action where
            // the SDK has it (macOS 26), the _np form before it (macOS
            // 10.15, glibc 2.29)
            int _addchdir(const char* dir) {
#if defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 260000
                return ::posix_spawn_file_actions_addchdir(&actions, dir);
#elif defined(__APPLE__) || defined(__GLIBC__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                return ::posix_spawn_file_actions_addchdir_np(&actions, dir);
#pragma clang diagnostic pop
#else
                (void)dir;
                return ENOTSUP;
#endif
            }

            expected<void, error> _dup(int fd, int target) {
                if (fd < 0) {
                    return detail::fail(last_error("start", cmd.path));
                }
#ifdef __APPLE__
                int rc = fd == target ? ::posix_spawn_file_actions_addinherit_np(&actions, fd) : ::posix_spawn_file_actions_adddup2(&actions, fd, target);
#else
                int rc = ::posix_spawn_file_actions_adddup2(&actions, fd, target);
#endif
                if (rc != 0) {
                    errno = rc;
                    return detail::fail(last_error("start", cmd.path));
                }
                return {};
            }

            static async::task<void> _copy_in(io::reader from, tracked_ptr<file> to) {
                (void)co_await io::async_copy(to, from);
                (void)to->close();   // the child sees the end of its input
            }

            static async::task<void> _copy_out(io::writer to, tracked_ptr<file> from) {
                (void)co_await io::async_copy(to, from);
            }
        };

        // The stop requested: the child killed, unless it ended first
        static async::task<void> _watch_stop(tracked_ptr<io::process> p, async::stop_token stop, tracked_ptr<async::channel<void>> done) {
            bool stopped = false;
            co_await async::select(stop.on_stop([&] { stopped = true; }), done->on_receive([] {}));
            if (stopped) {
                (void)p->kill();
            }
        }

        // The copying tasks awaited, within wait_delay when set: past
        // it the program's pipe ends are closed, which ends them, and
        // the wait reports errc::wait_delay
        async::task<expected<void, error>> _await_copies() {
            expected<void, error> r;
            if (wait_delay == duration::zero()) {
                for (auto& c : _copies) {
                    co_await c;
                }
            } else {
                auto deadline = clock::now() + wait_delay;
                for (auto& c : _copies) {
                    auto left = deadline - clock::now();
                    if (left <= duration::zero() || !co_await async::with_timeout(std::move(c), left)) {
                        for (auto& p : _pipes) {
                            (void)p->close();
                        }
                        r = detail::fail(error(errc::wait_delay, "wait", path));
                        break;
                    }
                }
            }
            _copies.clear();
            co_return r;
        }

        expected<void, error> _finish(expected<process_state, error> ended, expected<void, error> copies) {
            if (_done) {
                _done->close();
            }
            for (auto& p : _pipes) {   // the program's ends of the pipes: an input's, so that the child... has ended already; an output's, drained by its task
                (void)p->close();
            }
            _pipes.clear();
            if (!ended) {
                return detail::fail(ended);
            }
            state = *ended;
            if (!copies) {
                return copies;
            }
            if (!ended->success()) {
                return detail::fail(error(errc::exit_status, "wait", path));
            }
            return {};
        }

        expected<tracked_ptr<buffer>, error> _capture_output() {
            if (out || process) {
                return detail::fail(error(std::make_error_code(std::errc::invalid_argument), "output", path));
            }
            tracked_ptr<buffer> captured = make_tracked<buffer>();
            out = captured;
            if (!err) {
                _err_capture = make_tracked<buffer>();
                err = _err_capture;
            }
            return captured;
        }

        expected<tracked_ptr<buffer>, error> _capture_combined() {
            if (out || err || process) {
                return detail::fail(error(std::make_error_code(std::errc::invalid_argument), "combined_output", path));
            }
            tracked_ptr<buffer> captured = make_tracked<buffer>();
            out = captured;
            err = captured;
            return captured;
        }

        expected<string, error> _captured(expected<void, error> r, const buffer& captured) {
            if (_err_capture) {
                captured_err = _err_capture->text();
                _err_capture = nullptr;
            }
            if (!r) {
                return detail::fail(r);
            }
            return captured.text();
        }

        // A pipe end the child will read or write: blocking, as a
        // program expects its standard streams (pipe() makes both ends
        // non-blocking for the reactor; the flag is the end's own, the
        // program's end keeps it)
        static void _child_end(const file& f) {
            int flags = ::fcntl(f.fd(), F_GETFL);
            if (flags >= 0) {
                ::fcntl(f.fd(), F_SETFL, flags & ~O_NONBLOCK);
            }
        }

        vector<tracked_ptr<file>> _child_ends;    // the child's ends of the pipes made by stdin_pipe and the others, closed by start() after the spawn
        vector<tracked_ptr<file>> _pipes;         // the program's ends of the pipes start() made
        vector<async::task<void>> _copies;               // the tasks copying to and from them
        tracked_ptr<async::channel<void>> _done;         // closed by wait: the stop watcher ends
        tracked_ptr<buffer> _err_capture;         // output(): the standard error, when not given

        // the two halves of the operations above: a thread's and a task's
        expected<void, error> _block_wait() {
            if (!process) {
                return detail::fail(error(errc::process_done, "wait", path));
            }
            auto ended = process->_block_wait();
            if (_copies.empty()) {
                return _finish(std::move(ended), {});
            }
            return _finish(std::move(ended), async::spawn(_await_copies()).wait());
        }

        async::task<expected<void, error>> _co_wait()  {
            if (!process) {
                co_return detail::fail(error(errc::process_done, "wait", path));
            }
            auto ended = co_await process->async_wait();
            co_return _finish(std::move(ended), co_await _await_copies());
        }

        expected<void, error> _block_run()  {
            if (auto s = start(); !s) {
                return s;
            }
            return _block_wait();
        }

        async::task<expected<void, error>> _co_run()  {
            if (auto s = start(); !s) {
                co_return s;
            }
            co_return co_await async_wait();
        }

        expected<string, error> _block_output()  {
            auto captured = _capture_output();
            if (!captured) {
                return detail::fail(captured);
            }
            auto r = _block_run();
            return _captured(std::move(r), **captured);
        }

        async::task<expected<string, error>> _co_output()  {
            auto captured = _capture_output();
            if (!captured) {
                co_return detail::fail(captured);
            }
            auto r = co_await async_run();
            co_return _captured(std::move(r), **captured);
        }

        expected<string, error> _block_combined_output()  {
            auto captured = _capture_combined();
            if (!captured) {
                return detail::fail(captured);
            }
            auto r = _block_run();
            return _captured(std::move(r), **captured);
        }

        async::task<expected<string, error>> _co_combined_output()  {
            auto captured = _capture_combined();
            if (!captured) {
                co_return detail::fail(captured);
            }
            auto r = co_await async_run();
            co_return _captured(std::move(r), **captured);
        }
    };
}
