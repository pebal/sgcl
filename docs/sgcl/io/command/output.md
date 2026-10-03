[sgcl](../../README.md) › [io](../README.md) › [command](../command.md)

# sgcl::io::command::output, async_output

```cpp
expected<string, error> output();                                // (1)
async::task<expected<string, error>> async_output() noexcept;    // (2)
```

Runs the child with its standard output captured and returns what it wrote, Go's `Cmd.Output`. When `err` is empty,
the standard error is captured too, into `captured_err`, for the message of a failure (Go's `ExitError.Stderr`).
`out` must be empty, and the command not started.

1. Waits on the calling thread.
2. The same as a task, the end of the child waited for on the [reactor](../../async/exited.md): twenty children
   asked at once are twenty registrations on the reactor, and no thread is held by any of them.

## Parameters

None.

## Return value

The standard output of the child, or the [error](../error.md): `std::errc::invalid_argument` (operation `output`)
when `out` is set or the command was started, else the error of [run](run.md) (`errc::exit_status` for a failure
status: what the child wrote to its standard error is in `captured_err`).

## Complexity

The time of the child, plus linear in what it writes.

## Exceptions

- (1) `std::system_error` when the threads of the scheduler, which the start or the wait may start, cannot be made;
  `length_error` when the output passes 4 GiB, the most a string holds.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command tr("tr", "a-z", "A-Z");
    tr.in = io::buffer("quiet\n");  // copied to the child through a pipe by a task
    if (auto upper = tr.output()) {
        print("{}", *upper);
    }

    io::command cat("cat", "no-such-file");
    if (auto out = cat.output(); !out) {
        println("{}", out.error().is_exit_status());
        println("{}", cat.captured_err.trim());
    }
}
```

Output:

```text
QUIET
true
cat: no-such-file: No such file or directory
```

Several children at once, from a task.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<vector<string>> greetings(vector<string> names) {
    vector<io::command> commands;
    for (const string& name : names) {
        commands.push_back(io::command("echo", "hello,", name));
    }
    vector<async::task<expected<string, io::error>>> asked;
    for (io::command& c : commands) {
        asked.push_back(async::spawn(c.async_output()));  // all at once
    }
    vector<string> answers;
    for (auto& a : asked) {
        auto v = co_await a;
        answers.push_back(v ? string(v->trim()) : string("?"));
    }
    co_return answers;
}

int main() {
    for (const string& line : async::run(greetings({"Ada", "Grace"}))) {
        println(line);
    }
}
```

Output:

```text
hello, Ada
hello, Grace
```

## See also

- [combined_output](combined_output.md): the standard output and error together
- [run](run.md): the run, its output where `out` says
- [stdout_pipe](stdout_pipe.md): the output read by the program as the child writes it
- [sgcl::io::command](../command.md)
