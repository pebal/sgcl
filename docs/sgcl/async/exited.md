[sgcl](../README.md) › [async](README.md)

# sgcl::async::exited

```cpp
#include "sgcl/async/reactor.h"   // or "sgcl/async.h"

namespace sgcl::async {
    event exited(int pid);
}
```

Returns an [event](event.md) set when the process with the id `pid` has ended. `co_await async::exited(pid)` holds no
thread while a child runs, and a `waitpid` after it does not block: the status, still to be collected, says how the
child ended. A process that has ended already, or that does not exist, sets the event at once. It is what
[io::process](../io/process.md) waits on, so that a child's wait holds no thread, as Go 1.23 waits on Linux.

On kqueue the wait is a one-shot registration of the reactor (`EVFILT_PROC`), with the rules of
[readable](readable.md): a stop of the reactor ends it with nothing. On Linux it is for now a thread per process,
in `waitid` with `WNOWAIT`, which leaves the status for `waitpid`; a pidfd on the reactor is to come.

## Parameters

| Parameter | Description |
|---|---|
| `pid` | the id of the process, a child of this one for its status to be collected |

## Return value

An event, set when the process has ended, or when the wait is ended with nothing.

## Complexity

Constant: the event made, and the registration with the kernel under the reactor's lock.

## Exceptions

`std::system_error` when the reactor's thread, or on Linux the thread of the wait, has to be started and cannot be.

## Notes

The event says the process ended, or that the wait was ended with nothing (a stop of the reactor), after which the
child may still run and a `waitpid` blocks. How it ended is in the status the program collects:
`io::process::wait`, or `waitpid`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> watch(int pid) {
    co_await async::exited(pid);  // no thread held while the child runs
    println("the child has ended");
}

int main() {
    io::command child("sleep", "0.05");
    println("started: {}", child.start().has_value());
    async::spawn(watch(child.process.pid())).wait();
    println("collected: {}", child.wait().has_value());
    println("exit code {}", child.state->exit_code());
}
```

Output:

```text
started: true
the child has ended
collected: true
exit code 0
```

## See also

- [io::command](../io/command.md), [io::process](../io/process.md): a child process, whose wait this is
- [readable](readable.md): the reactor's waits and their rules
- [event](event.md): what a wait is
