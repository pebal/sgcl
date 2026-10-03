[sgcl](../../README.md) › [io](../README.md) › [command](README.md)

# sgcl::io::command::run, async_run

```cpp
expected<void, error> run();                                // (1)
async::task<expected<void, error>> async_run() noexcept;    // (2)
```

Starts the child and waits for it to end, Go's `Cmd.Run`: [start](start.md), then [wait](wait.md).

1. Waits on the calling thread.
2. The same as a task, the end of the child waited for on the [reactor](../../async/exited.md).

## Parameters

None.

## Return value

Nothing when the child exited with 0, or the first [error](../error/README.md): the error of `start`, or that of `wait`
(`errc::exit_status` for a failure status, the code in `state`).

## Complexity

The time of the child.

## Exceptions

- (1) `std::system_error` when the threads of the scheduler, which the start or the wait may start, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::command build("/bin/sh", "-c", "echo building; exit 2");
    build.out = io::stdout;  // the program's own descriptors, inherited
    build.err = io::stderr;
    if (auto ran = build.run(); !ran) {
        println("{}: {}", ran.error().message(), build.state ? build.state->to_string() : string());
    }

    io::command later("true");
    auto ran = async::run(later.async_run());
    println("{}", bool(ran));
}
```

Output:

```text
building
wait /bin/sh: the process ended with a failure status: exit status 2
true
```

## See also

- [output](output.md): the run with the standard output captured
- [start](start.md), [wait](wait.md): the two halves
- [sgcl::io::command](README.md)
