[sgcl](../README.md) › [async](README.md)

# sgcl::async::with_timeout

```cpp
#include "sgcl/async/timeout.h"   // or "sgcl/async.h"

namespace sgcl::async {
    /*(1)*/ template<class T>
            task<expected<T, timed_out>> with_timeout(task<T> t, duration d);
    /*(2)*/ template<class T>
            task<expected<T, timed_out>> with_timeout(task<T> t, duration d, stop_source loser);
}
```

A timeout on a task: the task's result, or the error [timed_out](timed_out.md) when `d` passed first. It is what
the [timeout](timeout.md) case is for a select, and what Go writes as `select { case r := <-done: case
<-time.After(d): }` or with `context.WithTimeout`. A deadline that passed is a failure the library reports, so it
is an [expected](../core/expected.md), as every failure of the library is; a task of nothing gives
`expected<void, timed_out>`. What the task itself threw is the task's, not a failure of the timeout, and comes
through as it is.

1. The task raced against `d`.
2. The same, and the stop of `loser` requested when `d` passed first.

`with_timeout(t, d)` is [with_deadline](with_deadline.md)`(t, clock::now() + d)`: the deadline is counted from
the call, not from the first wait for the result. The result is a [task](task.md), waited for as one: awaited
(`co_await async::with_timeout(t, d)`), waited for from a thread (`async::with_timeout(t, d).wait()`), spawned or
not (a wait starts it).

A task cannot be stopped from outside, so the timeout does not stop the task that lost: it runs on to its end, its
result lands where nobody reads it any more, and the objects and the frames are the collector's. To have the
loser stop, it is given a token whose source the timeout may stop, (2): `async::with_timeout(t, d, source)`
requests the stop of `source` when `d` passes first, so a task made with `source.token()` sees it. A source
made as a child of the caller's token (`async::stop_source source(token)`) lets the caller's stop reach the task
as well.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to race, consumed: started if nobody started it |
| `d` | the time the task has |
| `loser` | the source whose stop is requested when the time passes first |

## Return value

A task whose result is the result of `t` as the value of an `expected<T, timed_out>`, or the error `timed_out`
when `d` passed first; `expected<void, timed_out>` for a `task<void>`. A result that is an `expected` itself
(a task of `expected<size_t, io::error>`) is the value of the outer one.

## Complexity

Constant: one managed object for the race, the frame of the timeout task, and a timer, whose push into a heap of
timers is logarithmic in the timers of that heap.

## Exceptions

The call: none; the task it returns starts suspended. Carried out: what `t` threw, rethrown as it is when `t` ended
first; `std::system_error` when the timer thread or the scheduler's workers cannot be started, and, in (2), when
the stop of `loser` wakes a task and the workers cannot be started.

## Notes

- The race against a duration is one managed object and no frame of its own: the raced task is given a
  continuation that is a call into the race instead of a coroutine to resume, the deadline is a timer that calls
  into it, and the first call wins with a compare-exchange on the race's word and hands the timeout task's frame
  to the scheduler; the second finds the word taken and does nothing. A task that won cancels the timer, which is
  swept out of the heap with the other cancelled ones; a deadline that won leaves the task to run on, its end
  calling into a race that is over.
- A result that came at the same instant as the deadline is a result: the task is looked at whichever call won.
- The loser's value or exception, when it ends, is dropped with the race, never [on_unhandled](on_unhandled.md)'s.
  `loser` is left alone when the task finished in time.
- A timeout of zero, or less, is a deadline that has passed: a task done already gives its result, any other is
  `timed_out` and runs on. A deadline of zero is still a timer: a task that finishes in the few microseconds before
  the timer thread fires it wins the race.
- The time is the module's clock, so a [manual_clock](manual_clock.md) serves the race.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

// A lookup that takes a while, or stops when its token says so
async::task<string> lookup(string name, duration takes, async::stop_token token) {
    size_t which = co_await async::select(
        async::timeout(takes, [] {}),  // the work: here, a wait
        token.on_stop([] {})
    );
    if (which == 1) {
        println("{} stopped", name);
    }
    co_return name + " found";
}

int main() {
    async::manual_clock clock;
    clock.install();
    async::stop_source fast_source, slow_source;
    auto fast = async::spawn(async::with_timeout(lookup("fast", 1s, fast_source.token()), 5s));
    auto slow = async::spawn(
        async::with_timeout(lookup("slow", 1min, slow_source.token()), 5s, slow_source));
    clock.advance(1s);  // the fast lookup ends
    clock.advance(4s);  // the deadline of both
    auto a = fast.wait();
    auto b = slow.wait();
    println("{}", a ? *a : a.error().message());
    println("{}", b ? *b : "slow " + b.error().message());
}
```

Output:

```text
slow stopped
fast found
slow timed out
```

## See also

- [with_deadline](with_deadline.md): the same by a point of the clock, or by a token's stop
- [timed_out](timed_out.md): the error
- [timeout](timeout.md): a wait bounded in a select
- [when_any](when_any.md): a race of tasks
- [task_group](task_group.md): a scope of tasks stopped as one
- [stop_source](stop_source.md): the source given for the loser
