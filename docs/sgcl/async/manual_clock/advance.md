[sgcl](../../README.md) › [async](../README.md) › [manual_clock](README.md)

# sgcl::async::manual_clock::advance

```cpp
void advance(duration d) noexcept;
```

Moves the manual time forward by `d`: [advance_to](advance_to.md)`(now() + d)`. Every timer due by the new time
fires, in the order of its deadline, and the call returns once the tasks those timers woke have reached their
next waits: a sleep of thirty seconds completes on `advance(30s)`, in microseconds of wall time.

The advance first waits until every task has reached its next wait (no task ready, none running, every worker
asleep), so that a task spawned a moment ago has armed its timer; then it moves the time and wakes the timer
thread, which fires every timer now due and reports back when nothing is; then it waits again until the tasks
woken have reached their next waits. A chain of `advance(1s)` runs a task that sleeps in a loop one lap per call.
A tick whose several periods fall into one advance fires once per period, and its channel holds one signal of
them, as for a slow receiver in real time.

The clock must be installed, and `d` must not be negative (assertions in debug builds): the time never goes back.

## Parameters

| Parameter | Description |
|---|---|
| `d` | how far to move the time |

## Return value

None.

## Complexity

The timers that fall due, each fired once (a tick once per period covered), and the waits for the workers to
settle on either side of the move.

## Exceptions

None. The calls of the timer thread's `std::mutex` and `std::condition_variable` fail only on a lock the caller
holds already, which the module never takes twice.

## Notes

The advance is called from a thread that is not a worker (the test's), since it waits for the workers to be idle.
A task that waits by spinning, or that holds a worker in a blocking call, holds the advance until it stops; a task
waiting on a channel, a timer, another task or a stop token holds nothing.

The timers due by the new time fire in one pass of the timer thread, in the order of their deadlines, without
waiting for the tasks they wake: a task that a timer at one second wakes, raced against a deadline at five, may
lose the race on `advance(5s)`, where `advance(1s)` and then `advance(4s)` let it run in between, as the real time
would.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> heartbeat(async::channel<int> beats) {
    for (int i : range(3)) {
        co_await async::sleep(1min);
        co_await beats.send(i + 1);
    }
}

int main() {
    async::manual_clock clock;
    clock.install();
    async::channel<int> beats(8);
    auto t = async::spawn(heartbeat(beats));
    for (int lap : range(3)) {
        clock.advance(1min);  // one lap of the loop per call
        println("after {} min: beat {}", lap + 1, *beats.try_receive());
    }
    t.wait();
}
```

Output:

```text
after 1 min: beat 1
after 2 min: beat 2
after 3 min: beat 3
```

## See also

- [advance_to](advance_to.md): to a point
- [now](now.md): the manual time
- [sgcl::async::manual_clock](README.md)
