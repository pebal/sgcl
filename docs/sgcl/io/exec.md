# sgcl::io::command, process, process_state, look_path

```cpp
#include "sgcl/io/exec.h"   // or "sgcl/io/io.h", "sgcl/sgcl.h"

namespace sgcl::io {
    class command;          // a program to run: the fields of exec.Cmd, start, wait, run, output
    class process;          // a running child: pid, signal, kill, wait, release (os.Process)
    class process_state;    // how a child ended: exit_code, signal, the times (os.ProcessState)
    expected<string, error> look_path(const string& file);   // the executable a name stands for (exec.LookPath)
}
```

What Go's `os/exec` does, in the shapes of the library. A `command` is a value: the program and its arguments from the constructor, the rest — a directory, an environment, the three standard streams — set by name in any order, as the fields of `exec.Cmd`; `start()` runs it, `wait()` collects it, `run()` is the two, `output()` the two with the standard output captured. The streams are the library's: a [file](file.md) in `in`, `out` or `err` is inherited by the child as a descriptor, any other [reader or writer](stream.md) is served through a pipe by a task that copies (Go's goroutines), a null pointer is the null device. The child is a `process`; how it ended is a `process_state`.

```cpp
io::command cmd("git", "status", "--short");
cmd.dir = "/repo";
auto out = cmd.output();                       // expected<string, error>: the standard output, or the error
if (!out) {
    std::cerr << out.error().message() << ' ' << cmd.captured_err;   // "wait /usr/bin/git: the process ended with a failure status"
}
```

The child is made with `posix_spawn`, never `fork`: a fork of a process with the collector's threads would hold their locks in a child that has none of them. Its exit is waited for on the [reactor](../async/reactor.md) (`async::exited(pid)`: kqueue's `EVFILT_PROC`, a pidfd on Linux), so `wait()` holds no thread while the child runs, as Go 1.23 waits on Linux; `wait()` from a thread is `waitpid`. A `command("true").run()` costs 1.1 ms on an Apple M-series core against Go's 1.9 (Go forks on macOS), 32 at once 0.4 ms each against 0.8 (`bench_io`, `benchmarks/go/exec`).

## Rules

- A `command` holds tracked pointers (the streams, the process, the pipes' tasks), so it lives where one may: on a stack, in a managed object, in a coroutine frame; never in `new` memory or a `std` container ([The rules](../core/README.md#the-rules), 1). It is moved, not copied.
- `start()` once; `wait()` once, from a thread (`cmd.wait()`, `waitpid`) or a task (`co_await cmd.async_wait()`, on the reactor). A child ended with a failure status is `errc::exit_status` from `wait` (Go's `ExitError`), the code in `state`; a child never waited for stays a zombie until the program ends, as everywhere.
- `in`, `out`, `err`: null is the null device (Go's nil), as `run()` leaves them; a `file` is the descriptor itself (`io::stdout` to share the program's); a `buffer` or any other stream goes through a pipe and a task, started by `start()` and joined by `wait()`. The same writer in `out` and `err` gets one pipe, so the child's two streams interleave as written.
- `stop`: a [stop_token](../async/stop_token.md) whose stop kills the child (`SIGKILL`), Go's `CommandContext`; `wait_delay` bounds what `wait()` gives the copying tasks after the child ended (a grandchild holding the pipe), then closes the pipes and reports `errc::wait_delay`.
- `process::signal` and `kill` after the wait are `errc::process_done`: the id may be another process's. A `process` is not waited for by its destructor (a wait in a destructor blocks the sweep) and is released by it.
- The environment is inherited when `env` is `nullopt`, and is exactly the list when it is set (no `HOME` unless listed); `dir` is the child's working directory (`posix_spawn_file_actions_addchdir`: macOS 10.15, glibc 2.29); `set_pgid` puts the child in a process group of its own, so that `kill(-pid, sig)` reaches it and its children.
- Windows: to come with the platform port (`CreateProcess`, the job object for the group).

## Members

### command

```cpp
template<class... Args> explicit command(const string& name, const Args&... args);   // the program and its arguments (exec.Command)
command(const string& name, vector<string> args);
command(command&&) noexcept;  command& operator=(command&&) noexcept;               // moved, not copied

string path;                                    // the program: as given, then what look_path found
vector<string> args;                            // the arguments after the program's name (not argv[0])
optional<string> argv0;                         // argv[0] when not the name as given
optional<vector<pair<string, string>>> env;     // nullopt: inherited; a list: the whole environment
string dir;                                     // the working directory; empty: the program's own
io::reader in;                                  // standard input: none the null device, a stream with a descriptor (a file, io::stdin) that descriptor, any other a pipe and a task
io::writer out;                                 // standard output: the same (io::stdout shares the program's)
io::writer err;                                 // standard error: the same; the same stream as out shares its pipe
bool set_pgid = false;                          // a process group of its own (Setpgid)
async::stop_token stop;                                // the child killed on the stop (CommandContext)
duration wait_delay = duration::zero();         // wait()'s patience with the copying tasks after the child ended; zero: unbounded (WaitDelay)

tracked_ptr<process> process;                   // the child, after start()
optional<process_state> state;                  // how it ended, after wait()
string captured_err;                            // output(): the standard error, when err was null (ExitError.Stderr)

expected<void, error> start();                           // look_path, the streams arranged, posix_spawn; once
expected<void, error> wait();                            // waitpid on this thread, the tasks joined, the pipes closed; errc::exit_status for a failure status
async::task<expected<void, error>> async_wait();           // the same in a task: the exit on the reactor
expected<void, error> run();  async::task<expected<void, error>> async_run();                               // start and wait
expected<string, error> output();  async::task<expected<string, error>> async_output();                     // run with the standard output captured; the standard error into captured_err when err is null
expected<string, error> combined_output();  async::task<expected<string, error>> async_combined_output();   // both streams into one string, in the order written
expected<tracked_ptr<file>, error> stdin_pipe();         // a pipe the child reads: the program's end, closed by wait()
expected<tracked_ptr<file>, error> stdout_pipe();        // a pipe the child writes: read to its end before wait(), which closes it
expected<tracked_ptr<file>, error> stderr_pipe();
```

```cpp
io::command tr("tr", "a-z", "A-Z");
tr.in = make_tracked<buffer>(string("quiet\n"));   // a buffer: copied through a pipe by a task
auto upper = tr.output();                              // "QUIET\n"

io::command build("make", "-j8");
build.dir = "/project";
build.out = io::stdout;                              // the program's own descriptors, inherited
build.err = io::stderr;
build.stop = source.token();                           // the stop kills it
if (auto r = build.run(); !r) {
    std::cerr << (build.state ? build.state->str() : r.error().message()) << '\n';   // "exit status 2", "signal: killed"
}

io::command grep("grep", "error");
auto lines = grep.stdin_pipe();                        // the program writes what the child reads
auto found = grep.stdout_pipe();                       // and reads what it writes
grep.start();
(*lines)->write("no\nan error\n");
(*lines)->close();                              // the end of the input
auto hits = (*found)->read_all_text();                 // "an error\n", read before the wait
grep.wait();
```

From a task, the same with `async_`: `co_await cmd.async_output()` holds no thread while the child runs, twenty children at once are twenty registrations on the reactor.

```cpp
async::task<vector<string>> versions(vector<string> tools) {
    vector<async::task<expected<string, error>>> asked;
    vector<io::command> commands;
    for (auto& t : tools) {
        commands.push_back(io::command(t, "--version"));
    }
    for (auto& c : commands) {
        asked.push_back(async::spawn(c.output()));   // all at once
    }
    vector<string> out;
    for (auto& a : asked) {
        auto v = co_await a;
        out.push_back(v ? v->trim() : string("?"));
    }
    co_return out;
}
```

### process

```cpp
int pid() const noexcept;
expected<void, error> signal(int sig);                   // errc::process_done after the wait or the release
expected<void, error> kill();                            // SIGKILL
expected<process_state, error> wait();                   // waitpid, once
async::task<expected<process_state, error>> async_wait();   // the same in a task: exited(pid) on the reactor, then the status
expected<void, error> release();                         // let go of: nothing more from this object
```

### process_state

```cpp
int pid() const noexcept;
bool async::exited() const noexcept;                   // ended by exiting, not by a signal
int exit_code() const noexcept;                 // -1 when signaled
bool success() const noexcept;                  // exited with 0
bool signaled() const noexcept;  int signal() const noexcept;
std::chrono::microseconds user_time() const noexcept;  std::chrono::microseconds system_time() const noexcept;
string to_string() const;                       // "exit status 1", "signal: killed"
```

### look_path

```cpp
expected<string, error> look_path(const string& file);   // the name itself when it holds a '/' and is executable, else the first executable of the name in PATH; errc::not_found
```

### The errors

`errc::not_found` (no executable of the name), `errc::exit_status` (a failure status, `error::is_exit_status()`), `errc::process_done` (a second wait, a signal after the wait), `errc::wait_delay` (the copying tasks outlasted `wait_delay`), and `errno` from `posix_spawn`, `kill` and `waitpid` in the system category; the operation is `start`, `wait`, `signal` or `look_path`, the path the program.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// The words of a text counted by a pipeline of two children: sort feeds
// uniq -c through a pipe the program never sees, the text goes in from
// a buffer and the counts come back as lines
int main() {
    io::command sort("sort");
    io::command uniq("uniq", "-c");
    sort.in = make_tracked<io::buffer>(string("pear\napple\npear\nfig\napple\npear\n"));
    auto between = sort.stdout_pipe();               // sort writes here
    if (!between) {
        return 1;
    }
    uniq.in = *between;                              // and uniq reads it: a file, inherited as a descriptor
    tracked_ptr<io::buffer> counts = make_tracked<io::buffer>();
    uniq.out = counts;
    if (auto r = sort.start(); !r) {
        std::cerr << r.error().message() << '\n';
        return 1;
    }
    if (auto r = uniq.start(); !r) {
        std::cerr << r.error().message() << '\n';
        return 1;
    }
    (void)(*between)->close();                       // the program's copy of the pipe: uniq's is its own
    auto s = sort.wait();
    auto u = uniq.wait();
    if (!s || !u) {
        std::cerr << (s ? u : s).error().message() << '\n';
        return 1;
    }
    size_t lines = 0;
    for (string_slice line : counts->text().split('\n')) {
        if (!line.empty()) {
            std::cout << line.trim() << '\n';
            ++lines;
        }
    }
    std::cout << sort.state->str() << ", " << uniq.state->str() << '\n';
    return lines == 3 ? 0 : 1;
}
```

The output:

```
2 apple
1 fig
3 pear
exit status 0, exit status 0
```

## See also

- [file](file.md): `pipe`, the descriptors a child inherits; [stream](stream.md): `buffer`, the readers and writers a child is served through; [os](os.md): `io::stdin`, `io::stdout`, `io::stderr`, `environ()`
- [reactor](../async/reactor.md): `async::exited(pid)`; [stop_token](../async/stop_token.md); [blocking](../async/blocking.md)
- `tests/io/exec.cpp`; `benchmarks/io/io.cpp` and `benchmarks/go/exec`
