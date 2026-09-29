# sgcl::async::run

```cpp
namespace sgcl::async {
    template<class T> T run(task<T> t);                         // the task on the scheduler, waited for on this thread
    template<class F> auto run(F&& f);                          // f(stop_token) -> task<T>: the token stopped by SIGINT or SIGTERM
}
```

The entry of a program. `async::run(program())` puts the task on the scheduler, waits for it on the calling thread and gives back its value; what the task threw, `run` rethrows. It is `program().wait()` under the name a program starts with, as Go's `main` is where the goroutines start.

Given the coroutine function rather than its task, `run` calls it with a `stop_token` that the first SIGINT (Ctrl-C) or SIGTERM stops, the way Go's `signal.NotifyContext` does: the program sees the stop where it looks at its token (a case of its select, `co_await stop.stopped()`, `stop.stop_requested()`) and winds down on its own terms, closing its listeners, finishing the requests in flight, flushing what it wrote. The first signal also gives the two signals back what they did before `run`, so a second Ctrl-C, while the program is still winding down, does that: by default it ends the process, which the shell reports as 130 (143 for SIGTERM). A server that should never be cut short registers the signals itself ([signal](signal.md)) instead.

## Rules

- For a thread, `main` or another; never from a task, whose worker the wait would hold (debug builds assert, as `task::wait()` does).
- The signals are the module's ([signal](signal.md)): `run(f)` registers a channel of its own for SIGINT and SIGTERM and drops it at the first signal, or when the task ends, with a value or an exception. A channel the program registered for the same signals is not touched: it gets every signal, during `run` and after it, and while it is registered the signals keep the module's handler, so the second Ctrl-C goes to that channel rather than ending the process.
- The token is stopped once, by the first of the two signals; what the program does with the stop is its own. A program that ignores its token ends at the second signal.
- POSIX for now, as the signals are; Windows comes with the platform matrix.
- The function is called once, on the calling thread, and its task runs on the scheduler; a lambda with captures may be passed as it is, since it lives until `run` returns.

## Members

### run(task)

```cpp
template<class T> T run(task<T> t);
```

The task spawned if nobody spawned it, waited for, its value moved out; `run(t)` of a `task<void>` returns nothing. What the task threw is rethrown.

### run(f)

```cpp
template<class F> requires std::invocable<F&, stop_token> auto run(F&& f);
```

`f(token)` is a `task<T>`; `run` returns its value as above. Before it calls `f`, it registers SIGINT and SIGTERM and starts the task that watches them: the first signal stops the token and drops the registration. The registration is dropped as well when the task ends.

## Example

```cpp
#include "sgcl/async/async.h"
#include "sgcl/io/io.h"

#include <csignal>

using namespace sgcl;
using namespace std::chrono_literals;

// A loop that works at a fixed rate until the token stops: the ticks, and
// the stop as one more case of the select. After the third tick the
// program sends itself the SIGINT that Ctrl-C would.
async::task<int> serve(async::stop_token stop) {
    async::channel<void> every = async::tick(10ms);
    int ticks = 0;
    bool running = true;
    while (running) {
        co_await async::select(
            every.on_receive([&] {
                if (++ticks == 3) {
                    std::raise(SIGINT);                        // Ctrl-C
                }
            }),
            stop.on_stop([&] { running = false; })
        );
    }
    every.close();
    println("stopped: {}, at least 3 ticks: {}", stop.stop_requested(), ticks >= 3);
    co_return 0;
}

int main() {
    return async::run([](async::stop_token stop) { return serve(stop); });   // 0, the task's value
}
```

Output:

```text
stopped: true, at least 3 ticks: true
```

## See also

- [signal](signal.md): the channel of the signals under `run(f)`; [stop_token](stop_token.md): what the program looks at; [coroutine](coroutine.md): `task::wait()`, which `run` is
- `tests/async/run.cpp`: every behaviour above, checked, the second signal in a death test.
