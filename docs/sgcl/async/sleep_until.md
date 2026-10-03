[sgcl](../README.md) › [async](README.md)

# sgcl::async::sleep_until

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class [[nodiscard]] sleep_until {
    public:
        explicit sleep_until(time_point t) noexcept;

        bool await_ready() const noexcept;
        template<class P>
        void await_suspend(std::coroutine_handle<P> h);
        void await_resume() const noexcept;

        void wait() const;
    };
}
```

Waits until the point `t` of the module's clock. `co_await async::sleep_until(t)` suspends the task and holds no
thread meanwhile, as [sleep](sleep.md) does; `async::sleep_until(t).wait()` blocks the calling thread, through a
timer, so that a [manual_clock](manual_clock.md) serves it. A point that has passed neither suspends nor blocks.
A point is the same for a task and a thread, which is what it is for: a deadline computed once and slept to by
several.

`sleep_until` is a class called like a function: the object is the awaitable, made by the call and carried out by
`co_await` or `wait()`, and nothing is armed until then. Marked nodiscard, it does nothing when dropped.

A point is a `time_point` of the library's [clock](../core/clock.md), `sgcl::clock::now()`: the steady clock's
`time_point` (`sgcl::time_point`), or the manual clock's time while a test has one installed. A point of the
system clock (a calendar time) is converted by the program: `sgcl::clock::now() + (when -
std::chrono::system_clock::now())`. A timer at `time_point::max()` never fires.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the point to wait until, of `sgcl::clock` |

## Return value

An awaitable. Carried out, it gives nothing: `co_await async::sleep_until(t)` in a task,
`async::sleep_until(t).wait()` on a thread, each returning once `t` has come.

## Complexity

The call: constant, nothing armed. Carried out: one timer on the managed heap, pushed into a heap of timers:
logarithmic in the timers of that heap. `wait()` makes a channel for the thread to block on as well.

## Exceptions

The call: none. Carried out: `std::system_error` when the timer thread cannot be started.

## Notes

`co_await` is for a coroutine with a managed frame, a [task](task.md) or a [generator](generator.md) of the
module. A task asleep is held by its timer until the point comes, as [sleep](sleep.md#notes) says, with the
rest of what the timers share.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<string> woken(time_point deadline) {
    co_await async::sleep_until(deadline);
    co_return "the task";
}

int main() {
    time_point deadline = sgcl::clock::now() + 20ms;
    auto t = async::spawn(woken(deadline));
    async::sleep_until(deadline).wait();  // this thread, until the same point
    println("the thread: {}", sgcl::clock::now() >= deadline);
    println("{}: {}", t.wait(), sgcl::clock::now() >= deadline);
    async::sleep_until(deadline).wait();  // passed: returns at once
}
```

Output:

```text
the thread: true
the task: true
```

## See also

- [sleep](sleep.md): for a while
- [at](at.md): an event set at a point
- [timeout](timeout.md): a point as the deadline of a select
- [stop_source::stop_at](stop_source/stop_at.md): the stop requested at a point
- [clock](../core/clock.md): the clock the points are of
