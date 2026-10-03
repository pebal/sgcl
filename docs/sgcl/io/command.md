[sgcl](../README.md) › [io](README.md)

# sgcl::io::command

```cpp
#include "sgcl/io/exec.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class command;
}
```

`io::command` is a program to run, Go's `exec.Cmd`, in the shapes of the library. The constructor takes the program
and its arguments (`exec.Command`); the rest (a directory, an environment, the three standard streams, a stop) is
set by name, in any order, before the start, as the fields of `exec.Cmd` are. [start](command/start.md) runs it,
[wait](command/wait.md) collects it, [run](command/run.md) is the two, [output](command/output.md) the two with the
standard output captured. The child started is a [process](process.md), Go's `os.Process`; how it ended, a
[process_state](process_state.md), Go's `os.ProcessState`; the executable a name stands for, [look_path](look_path.md).

The streams of the child are the library's. A stream with a descriptor of its own in `in`, `out` or `err` (a
[file](file.md), a pipe's end, `io::stdout.file()`) is inherited by the child as that descriptor; any other
[reader](reader.md) or [writer](writer.md) (a [buffer](buffer.md), a connection, a stream of the program's) is served
through a pipe by a task that copies, as Go serves one by a goroutine; an empty stream is the null device, Go's
`nil`. The same writer in `out` and `err` gets one pipe, so the child's two streams interleave as it writes them.

The child is made with `posix_spawn`, never `fork`: a fork of a process with the collector's threads would hold their
locks in a child that has none of them. Its exit is waited for on the [reactor](../async/exited.md)
(`async::exited(pid)`: kqueue's `EVFILT_PROC`, a pidfd on Linux), so `co_await async_wait()` holds no thread while the
child runs, as Go 1.23 waits on Linux, and twenty children at once are twenty registrations on the reactor; `wait()`
on a thread is `waitpid`. A `command` is a value, as `exec.Cmd` is, not a handle: moved, never shared, each start of
a command of its own another child. What it costs against Go is on [benchmarks](benchmarks.md).

## Rules

- A `command` holds tracked pointers (the streams, the process, the copying tasks), so it lives where one may: on a
  stack, in a managed object, in a coroutine frame; never in `new` memory or a `std` container
  ([The rules](../core/README.md#the-rules), 1). It is moved, not copied: it owns the tasks that serve its pipes.
- `start()` once; `wait()` once, from a thread (`wait()`, `waitpid`) or a task (`co_await async_wait()`, on the
  reactor). A child that ended with a failure status is `errc::exit_status` from `wait` (Go's `ExitError`), the code
  in `state`. A child never waited for stays a zombie until the program ends, as everywhere.
- The environment is inherited when `env` is `nullopt`, and is exactly the list when it is set (no `HOME` unless
  listed). `dir` is the child's working directory, set by `posix_spawn_file_actions_addchdir` (macOS 10.15, glibc
  2.29).
- `stop`: a [stop_token](../async/stop_token.md) whose stop kills the child (`SIGKILL`), Go's `CommandContext`;
  `wait_delay` bounds what `wait()` gives the copying tasks after the child ended (a grandchild holding the pipe),
  then closes the pipes and reports `errc::wait_delay`.
- `set_pgid` puts the child in a process group of its own, so that `kill(-pid, sig)` reaches it and its children.
- Windows: to come with the platform port (`CreateProcess`, the job object for the group).

## Member objects

| Member | Description |
|---|---|
| `string path` | the program: as given, then what [look_path](look_path.md) found, after `start()` |
| `vector<string> args` | the arguments after the program's name, not `argv[0]` |
| `optional<string> argv0` | `argv[0]` when it is not the name as given |
| `optional<vector<pair<string, string>>> env` | the environment: `nullopt`, the default, is the program's own, inherited; a list is the child's whole environment |
| `string dir` | the working directory of the child; empty, the default, is the program's own |
| `io::reader in` | the standard input: empty, the default, the null device; a stream with a descriptor, that descriptor; any other reader, a pipe and a task |
| `io::writer out` | the standard output, the same way; `io::stdout.file()` shares the program's |
| `io::writer err` | the standard error, the same way; the same stream as `out` shares its pipe |
| `bool set_pgid` | the child in a process group of its own (Go's `Setpgid`); `false` by default |
| `async::stop_token stop` | the child killed when the stop is requested (Go's `CommandContext`); none by default |
| `duration wait_delay` | how long `wait()` gives the copying tasks after the child ended or the stop came, before it closes the pipes (Go's `WaitDelay`); zero, the default, waits for them |
| `io::process process` | the child, after `start()`; empty (`!process`) before |
| `optional<process_state> state` | how the child ended, after `wait()` |
| `string captured_err` | after `output()`: what the child wrote to its standard error when `err` was empty (Go's `ExitError.Stderr`) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](command/command.md) | the program and its arguments |
| [operator=](command/operator_assign.md) | moves a command |
| [start](command/start.md) | starts the child |
| [wait, async_wait](command/wait.md) | waits for the child to end |
| [run, async_run](command/run.md) | starts the child and waits for it |
| [output, async_output](command/output.md) | runs the child, its standard output captured |
| [combined_output, async_combined_output](command/combined_output.md) | runs the child, its standard output and error captured together |
| [stdin_pipe](command/stdin_pipe.md) | a pipe the child reads from |
| [stdout_pipe](command/stdout_pipe.md) | a pipe the child writes its standard output to |
| [stderr_pipe](command/stderr_pipe.md) | a pipe the child writes its standard error to |

## Example

The words of a text counted by a pipeline of two children: `sort` feeds `uniq -c` through a pipe the program never
reads, the text goes in from a buffer and the counts come back into another.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command sort("sort");
    io::command uniq("uniq", "-c");
    sort.in = io::buffer("pear\napple\npear\nfig\napple\npear\n");
    io::file between = sort.stdout_pipe().value();  // sort writes here
    uniq.in = between;  // and uniq reads it: a file, inherited as a descriptor
    io::buffer counts;
    uniq.out = counts;
    if (auto started = sort.start(); !started) {
        eprintln(started.error().message());
        return 1;
    }
    if (auto started = uniq.start(); !started) {
        eprintln(started.error().message());
        return 1;
    }
    (void)between.close();  // the program's copy of the pipe: uniq's is its own
    auto sorted = sort.wait();
    auto counted = uniq.wait();
    if (!sorted || !counted) {
        eprintln((sorted ? counted : sorted).error().message());
        return 1;
    }
    for (string_slice line : counts.text().split('\n')) {
        if (!line.empty()) {
            println(line.trim());
        }
    }
    println("{}, {}", sort.state->to_string(), uniq.state->to_string());
}
```

Output:

```text
2 apple
1 fig
3 pear
exit status 0, exit status 0
```

## See also

- [process](process.md), [process_state](process_state.md): the child and how it ended
- [look_path](look_path.md): the executable a name stands for
- [pipe](pipe.md): a pipe of the program's; [buffer](buffer.md): the bytes a child reads or writes in memory
- [standard_stream](standard_stream.md): `io::stdin`, `io::stdout`, `io::stderr`; [environ](environ.md): the
  environment to copy into `env`
- [exited](../async/exited.md): `async::exited(pid)`, the wait for a child on the reactor; [stop_token](../async/stop_token.md)
