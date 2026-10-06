[sgcl](../README.md) › [async](README.md)

# sgcl::async::sleep

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class [[nodiscard]] sleep {
    public:
        explicit sleep(duration d) noexcept;

        bool await_ready() const noexcept;
        template<class P>
        void await_suspend(std::coroutine_handle<P> h);
        void await_resume() const noexcept;

        void wait() const;
    };
}
```

Waits for `d`, the way Go's `time.Sleep` does. `co_await async::sleep(d)` suspends the task and holds no thread
meanwhile: the task is a frame on the managed heap and a timer, and a worker runs it again when the time comes.
`async::sleep(d).wait()` blocks the calling thread instead, through a timer as well, so that a
[manual_clock](manual_clock/README.md) serves a thread's sleep as it serves a task's. A `d` of zero or less neither
suspends nor blocks.

`sleep` is a class called like a function: the object is the awaitable, made by the call and carried out by
`co_await` or `wait()`, and nothing is armed until then. Marked nodiscard, it does nothing when dropped.

The span is a [duration](../core/duration/README.md): the literals of `<chrono>` (`500ms`, `2s`) and any integral
`std::chrono` duration convert into it. A span too long for the clock saturates: `clock::now() + d` stops at
`time_point::max()`, a point that never fires, so `async::sleep(duration::max())` waits forever, where the point
would otherwise wrap into the past and fire at once.

## Parameters

| Parameter | Description |
|---|---|
| `d` | how long to wait |

## Return value

An awaitable. Carried out, it gives nothing: `co_await async::sleep(d)` in a task, `async::sleep(d).wait()` on a
thread, each returning once `d` has passed.

## Complexity

The call: constant, nothing armed. Carried out: one timer on the managed heap, pushed into a heap of timers
(one heap per worker under a lock of its own, one shared by the other threads): logarithmic in the timers of that
heap. `wait()` makes a channel for the thread to block on as well.

## Exceptions

The call: none. Carried out: `std::system_error` when the timer thread cannot be started (the first timer of the
program starts it, and the first after a [scheduler::stop](scheduler/README.md)).

## Notes

`co_await` is for a coroutine with a managed frame: a [task](task/README.md) or a [generator](generator/README.md) of the
module. A thread sleeps with `wait()`, or with `sgcl::this_thread::sleep_for`, the operating system's, which a
manual clock does not serve.

A task that sleeps lives until its sleep ends, even when nobody waits for it any more: its timer holds its frame,
as a goroutine asleep is not collected either. A task that may be abandoned (the loser of a
[with_timeout](with_timeout.md), a task nobody awaits) waits in a [select](select.md) with its
[stop_token](stop_token/README.md)'s `on_stop` beside a [timeout](timeout.md) instead.

The timers of the module (`sleep`, [sleep_until](sleep_until.md), [after](after.md), [at](at.md),
[tick](tick.md), [timeout](timeout.md), a [stop_source](stop_source/README.md)'s deadline) are served by one thread,
asleep until the earliest timer is due and woken by a new timer only when it is the earliest. The thread starts
with the first timer and stops with the scheduler ([scheduler::stop](scheduler/README.md), or the end of the program);
a timer armed while the stop is joining the thread stays in its heap, as one not yet due does, and fires once the
next timer starts the thread again. A timer is a managed object held by a root in the heap: the frame it will
resume, or the channel it will signal, lives while the timer does, and nothing else holds them for it. Firing a
timer is a push on the scheduler or a signal on a channel; the thread never runs a body of the program. The thread
is a thread of the program to the collector, like any other.

A timer fires at its time or a little after, never before: the resolution is the steady clock's and the timer
thread's wake-up. On macOS the thread sleeps on a kernel timer marked critical, which the system does not delay to
fire it together with others (its timer coalescing lets an ordinary timed wait fire up to a quarter of its span
late, a kevent's timeout, Go's, an eighth); elsewhere on a condition variable, late by the system's timer slack.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> late(int value) {
    co_await async::sleep(20ms);  // the worker is free meanwhile
    co_return value;
}

int main() {
    time_point start = sgcl::clock::now();
    auto t = async::spawn(late(42));
    async::sleep(10ms).wait();  // this thread blocks
    println("{}", t.wait());
    println("{}", sgcl::clock::now() - start >= 20ms);
}
```

Output:

```text
42
true
```

A thousand tasks asleep at once on one worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> late(int value) {
    co_await async::sleep(20ms);
    co_return value;
}

int main() {
    async::scheduler::set_workers(1);
    time_point start = sgcl::clock::now();
    vector<async::task<int>> sleepers;
    for (int i : range(1000)) {
        sleepers.push_back(async::spawn(late(i)));
    }
    int sum = 0;
    for (auto& s : sleepers) {
        sum += s.wait();
    }
    println("{}", sum);
    println("{}", sgcl::clock::now() - start < 1s);
}
```

Output:

```text
499500
true
```

## See also

- [sleep_until](sleep_until.md): until a point of the clock
- [after](after.md): an event set after a while, for a select or several waiters
- [timeout](timeout.md): a wait bounded in a select
- [manual_clock](manual_clock/README.md): the time of a test
- [clock](../core/clock/README.md), [duration](../core/duration/README.md): the time and the span
