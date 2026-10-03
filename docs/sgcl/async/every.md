[sgcl](../README.md) › [async](README.md)

# sgcl::async::every

```cpp
#include "sgcl/async/every.h"   // or "sgcl/async.h"

namespace sgcl::async {
    /*(1)*/ template<class F>
            stop_source every(duration d, F f);
    /*(2)*/ template<class F>
            stop_source every(duration d, F f, const stop_token& parent);
}
```

Calls `f` every `d` until the [stop_source](stop_source.md) it returns is stopped: the loop over [tick](tick.md)
with a [select](select.md) on the stop that a program would otherwise write itself, in a task of its own started by
[go](go.md). `f` is called with no arguments: a function, called on a worker, or a coroutine function, whose task is
awaited before the next tick, so that two calls never overlap.

The ticks are those of `tick(d)`: the first after `d`, then every `d`. A tick that comes while `f` still runs is
held, one, and the others dropped, so a slow `f` is called less often and never in a backlog; it is called again at
once when it returns past a tick. A period of zero or less never ticks, as for `tick`, and `f` is never called.

1. The source is a new one: `request_stop()` on it, or on any copy of it, ends the calls.
2. The source is a child of `parent` ([stop_source](stop_source/stop_source.md)): the calls end with its own stop
   and with the parent's, so that the loops of a program stop with the token of [run](run.md). An empty `parent`
   is no parent, as (1).

- (1–2) A call that has started ends; none starts after the stop is seen, and the loop ends there, its tick channel
  closed so that the timer lets go. The source returned is the only way to stop the calls: a source dropped without
  a stop leaves them running, as a task let go of runs on.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the period; zero or less: no call |
| `f` | what is called every period, with no arguments; copied into the loop's task, its captures with it |
| `parent` | the token whose stop ends the calls too |

## Return value

The source that stops the calls: a handle, one tracked word, copied freely, on a stack, in a task or in a managed
object, and in a global or a std container as a `rooted<async::stop_source>`.

## Complexity

Constant: the source's state, the loop's task and the tick's channel and timer on the managed heap. Each call is a
tick, a select and a wake of the task.

## Exceptions

`std::system_error` when the timer thread or a worker of the scheduler cannot be started; what the move constructor
of `F` throws.

What `f` throws ends the loop and goes, as from any task nobody waits for, to [on_unhandled](on_unhandled.md)'s
handler, which by default prints it and ends the program.

## Notes

A function `f` runs on a worker, which it holds while it runs: one that blocks (a disk, a lock held long) is a
coroutine function that awaits, or a call through [spawn_blocking](spawn_blocking.md). Under a
[manual_clock](manual_clock.md), an advance of a period calls `f` once and returns after the call.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();

    int beats = 0;
    auto heartbeat = async::every(1s, [&] { println("beat {}", ++beats); });
    clock.advance(1s);
    clock.advance(1s);
    clock.advance(1s);
    heartbeat.request_stop();
    clock.advance(5s);  // no more calls
    println("{} beats", beats);
}
```

Output:

```text
beat 1
beat 2
beat 3
3 beats
```

A coroutine function, stopped with the token of [run](run.md), in real time:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<bool> serve(async::stop_token stop) {
    tracked_ptr counter = make_tracked<atomic<int>>(0);  // managed: a call may outlive this frame
    auto flush = async::every(10ms, [counter]() -> async::task<> {
        co_await async::sleep(1ms);  // a write that waits, say
        counter->fetch_add(1);
    }, stop);
    co_await async::sleep(100ms);
    flush.request_stop();
    co_return counter->load() > 0;
}

int main() {
    println("{}", async::run(serve));
}
```

Output:

```text
true
```

## See also

- [tick](tick.md): the channel of the ticks, for a loop of its own
- [stop_source](stop_source.md): what stops the calls
- [go](go.md): the task the calls run in
- [manual_clock](manual_clock.md): the calls of a test
