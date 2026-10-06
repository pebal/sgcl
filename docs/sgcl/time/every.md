[sgcl](../README.md) › [time](README.md)

# sgcl::time::every

```cpp
#include "sgcl/time/cron.h"   // or "sgcl/time.h"

namespace sgcl::time {
    template<class F>
    async::stop_source every(const cron& c, F f);                                     // (1)
    template<class F>
    async::stop_source every(const cron& c, F f, const async::stop_token& parent);    // (2)
}
```

Calls `f` at every time of the [cron](cron/README.md) until the [stop_source](../async/stop_source/README.md) it
returns is stopped: [async::every](../async/every.md) with a cron's times for the period, in a task of its own started
by [go](../async/go.md). `f` is called with no arguments: a function, called on a worker, or a coroutine function,
whose task is awaited before the next time, so that two calls never overlap. A time that comes while `f` still runs
is held, one, and the others dropped: a slow `f` is called less often, never in a backlog.

1. Stopped by its source alone.
2. Stopped also when `parent` is: the source returned is its child.

The times are those of the wall clock in the cron's zone. The wait for one is a timer on the monotonic clock, read
again on waking, so a wall clock set back means a longer wait, not an early call; a test's
[manual_clock](../async/manual_clock/README.md) moves both clocks. A call that has started ends; none starts after
the stop is seen. What `f` throws goes where a task's nobody waits for goes: [on_unhandled](../async/on_unhandled.md).

Takes part only when `f` can be called with no arguments.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the cron whose times `f` is called at |
| `f` | the function, or the coroutine function |
| `parent` | the token whose stop stops the calls too |

## Return value

The source whose stop ends the calls.

## Complexity

Per time: a search for the next one ([next](cron/next.md)) and a timer.

## Exceptions

What the start of the task may throw: `std::system_error` when a thread of the scheduler cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;  // the time of the example: a minute takes no time at all
    clock.install();
    atomic<int> calls = 0;
    auto job = time::every(time::cron("* * * * *", time::zone::utc()), [&] { ++calls; });
    for (int i : range(5)) {
        clock.advance(60s);
    }
    job.request_stop();
    clock.advance(60s);
    println("{} calls in five minutes", calls.load());
}
```

Output:

```text
5 calls in five minutes
```

## See also

- [cron](cron/README.md): the times
- [async::every](../async/every.md): a function called every period
- [async::tick](../async/tick.md): a channel signalled every period
