[sgcl](../README.md) › [async](README.md)

# sgcl::async::run

```cpp
#include "sgcl/async/run.h"   // or "sgcl/async.h"

namespace sgcl::async {
    /*(1)*/ template<class T>
            T run(task<T> t);
    /*(2)*/ template<class F>
                requires std::invocable<F&, stop_token>
            auto run(F&& f);
}
```

The entry of a program: `int main() { return async::run(program()); }` puts the task on the scheduler, waits for it
on the calling thread and gives its value back; what the task threw, `run` rethrows. It is `program().wait()` under
the name a program starts with, as Go's `main` is where the goroutines start.

1. Starts `t` if nobody started it, waits for it, and returns its value, moved out of the frame; for a `task<void>`,
   nothing.
2. Calls `f` with a [stop_token](stop_token.md) that the first SIGINT (Ctrl-C) or SIGTERM stops, the way Go's
   `signal.NotifyContext` does, and runs the task `f` returns as (1). The program sees the stop where it looks at its
   token (a case of its [select](select.md), `co_await stop.stopped()`, `stop.stop_requested()`) and winds down on
   its own terms: it closes its listeners, finishes the requests in flight, flushes what it wrote. The first signal
   also gives the two signals back what they did before `run`, so a second Ctrl-C, while the program is still winding
   down, does that: by default it ends the process, which the shell reports as 130 (143 for SIGTERM).

Before it calls `f`, (2) registers a channel of its own for SIGINT and SIGTERM ([signals](signals.md)) and starts the
task that watches it: the first signal stops the token and drops the registration, which is dropped as well when the
task ends, with a value or with an exception. A channel the program registered for the same signals is not touched:
it gets every signal, during `run` and after it, and while it is registered the signals keep the module's handler, so
that the second Ctrl-C goes to that channel rather than ending the process. A server that should never be cut short
registers the signals itself instead.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task of the program |
| `f` | the function that makes the task of the program from a `stop_token`, called once, on the calling thread |

## Return value

The value of the task: `T` for (1), what the `task` that `f` returns gives for (2); nothing for a `task<void>`.

## Complexity

The time the task takes. (2) adds the registration of the two signals and a task that waits for them.

## Exceptions

- What the task threw, rethrown.
- `std::system_error` when the start of the task starts the scheduler and a worker's thread cannot be started.
- (2) What `f` throws, and what the registration of the signals throws ([signals](signals.md)).

## Notes

For a thread, `main` or another; never from a task, whose worker the wait would hold (debug builds assert it, as
[wait](task/wait.md) does). The token is stopped once, by the first of the two signals; what the program does with the
stop is its own, and a program that ignores its token ends at the second signal. The signals are POSIX's; Windows
comes with the platform matrix, and until then (2) throws there, as [signals](signals.md) does. `f` lives until
`run` returns, so a lambda with captures may be passed as it is.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> program() {
    println("working");
    co_return 0;
}

int main() {
    return async::run(program());  // 0, the task's value
}
```

Output:

```text
working
```

A program that winds down at Ctrl-C; here it sends itself the SIGINT that Ctrl-C would.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <csignal>

using namespace sgcl;

async::task<int> serve(async::stop_token stop) {
    println("serving");
    std::raise(SIGINT);  // Ctrl-C
    co_await stop.stopped();
    println("stopped: {}", stop.stop_requested());
    co_return 0;
}

int main() {
    return async::run([](async::stop_token stop) { return serve(stop); });
}
```

Output:

```text
serving
stopped: true
```

## See also

- [stop_token](stop_token.md): what the program looks at
- [signals](signals.md): the channel of the signals under (2)
- [wait, operator co_await](task/wait.md): the wait that `run` is
- [executor](executor.md): a program run as one task on the main thread
