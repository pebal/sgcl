[sgcl](../README.md) › [async](README.md)

# sgcl::async::with_deadline

```cpp
#include "sgcl/async/timeout.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class T>
    task<expected<T, timed_out>> with_deadline(task<T> t, time_point when);    // (1)
    template<class T>
    task<expected<T, timed_out>> with_deadline(task<T> t, time_point when,     // (2)
                                               stop_source loser);
    template<class T>
    task<expected<T, stopped>> with_deadline(task<T> t, stop_token token);     // (3)
}
```

A deadline on a task: the task's result, or an error when the deadline came first, Go's `context.WithDeadline`.
The result is an [expected](../core/expected.md), as every failure of the library is; a task of nothing gives
`expected<void, …>`. What the task itself threw comes through as it is.

1. The task raced against the point `when` of the module's clock; the error is [timed_out](timed_out.md).
2. The same, and the stop of `loser` requested when the point came first.
3. The task raced against the stop of `token`: a source given a [stop_after](stop_source/stop_after.md) or a
   [stop_at](stop_source/stop_at.md), or stopped by hand, Go's context handed down; the error is
   [stopped](stopped.md). The library cannot tell a deadline on the source from a stop by hand, as Go tells
   `DeadlineExceeded` from `Canceled`. An empty token (`async::stop_token()`) is no deadline: the result is the
   task's.

A point shared by several races, or a token, is one deadline for all of them. The result is a [task](task.md),
waited for as one: awaited, waited for from a thread (`.wait()`), spawned or not (a wait starts it).

A task cannot be stopped from outside, so a deadline does not stop the task that lost: it runs on to its end and
its result lands where nobody reads it any more. (2) stops `loser`, so a task made with `loser.token()` sees the
stop; under (3), a task made with the same token stops itself, and `with_deadline` stops nothing.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the task to race, consumed: started if nobody started it |
| `when` | the deadline, a point of `sgcl::clock` |
| `loser` | the source whose stop is requested when the deadline comes first |
| `token` | the token whose stop is the deadline |

## Return value

A task whose result is the result of `t` as the value of an `expected`, or its error when the deadline came first:

- (1–2) `expected<T, timed_out>`.
- (3) `expected<T, stopped>`.

A result that is an `expected` itself is the value of the outer one.

## Complexity

- (1–2) Constant: one managed object for the race, the frame of the timeout task, and a timer, whose push into a
  heap of timers is logarithmic in the timers of that heap.
- (3) Constant: the frames of the race and of a small runner task that reports the task's end into a slot, and a
  [select](select.md) between that slot's channel and the token's. Nothing is armed or cancelled: a token's stop is
  a channel closed.

## Exceptions

The call: none; the task it returns starts suspended. Carried out: what `t` threw, rethrown as it is when `t` ended
first; `std::system_error` when the timer thread or the scheduler's workers cannot be started, and, in (2), when
the stop of `loser` wakes a task and the workers cannot be started.

## Notes

- (1–2) are the race of [with_timeout](with_timeout.md#notes), which is `with_deadline(t, clock::now() + d)`: one
  managed object, the first of the task's end and the timer winning with a compare-exchange.
- A result that came at the same instant as the deadline is a result.
- The loser's value or exception, when it ends, is dropped with the race, never
  [on_unhandled](on_unhandled.md)'s. `loser` is left alone when the task finished in time.
- A point that has passed is a deadline that has passed: a task done already gives its result, any other is
  `timed_out` and runs on.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> step(duration takes, int value) {
    co_await async::sleep(takes);
    co_return value;
}

template<class E>
string show(const expected<int, E>& r) {
    return r ? to_string(*r) : r.error().message();
}

int main() {
    async::manual_clock clock;
    clock.install();
    time_point deadline = clock.now() + 10s;
    auto a = async::spawn(async::with_deadline(step(5s, 1), deadline));
    auto b = async::spawn(async::with_deadline(step(20s, 2), deadline));

    async::stop_source request;
    request.stop_after(10s);
    auto c = async::spawn(async::with_deadline(step(5s, 3), request.token()));
    auto d = async::spawn(async::with_deadline(step(20s, 4), request.token()));

    clock.advance(5s);  // the short steps end
    clock.advance(5s);  // the deadline
    println("{}, {}, {}, {}", show(a.wait()), show(b.wait()), show(c.wait()), show(d.wait()));
}
```

Output:

```text
1, timed out, 3, stopped
```

## See also

- [with_timeout](with_timeout.md): the same by a span
- [timed_out](timed_out.md), [stopped](stopped.md): the errors
- [stop_source::stop_after](stop_source/stop_after.md), [stop_source::stop_at](stop_source/stop_at.md): a deadline
  on a source
- [timeout](timeout.md): a point as the deadline of a select
