[sgcl](../README.md) › [async](README.md)

# sgcl::async::manual_clock

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class manual_clock;
}
```

The clock of a test: installed, it stops the module's time and moves it only when the test advances it. The library
reads the time in one place, [clock::now()](../core/clock.md) (core): the deadline a [sleep](sleep.md), an
[after](after.md), a [tick](tick.md), a [timeout](timeout.md) or a
[stop_after](stop_source/stop_after.md) computes, and the `now` the timer thread compares them with. It is the
steady clock's time unless a `manual_clock` is installed, and then it is the manual clock's, which stands still
until the test moves it: `advance(30s)` moves it thirty seconds forward, and every timer due by then fires, in the
order of its deadlines, with no waiting at all. This is what tokio's `time::pause()` and `advance()` and Kotlin's
`runTest` give: a test of a thirty-second timeout, of a retry every minute, of a deadline at midnight, takes
microseconds and is deterministic, because the time is not the machine's but the test's.

The wall time moves with it: what [time::now()](../time/datetime.md) reads while a manual clock is installed is
the wall time of the install moved on by as much as the manual time has been advanced, and a
`time::stopwatch` reads `sgcl::clock`, so code that asks for the time or measures itself is tested with no real
waiting as well.

An advance does three things, in order. It waits until every task has reached its next wait (no task ready, none
running, every worker asleep), so that a task spawned a moment ago has armed its timer; it moves the time and wakes
the timer thread, which fires every timer now due and reports back when nothing is; and it waits again until the
tasks those timers woke have reached their next waits, so that a task that sleeps in a loop has armed its next
sleep when the advance returns, and the next advance finds it. A chain of `advance(1s)` therefore runs a task
through its loop one lap per call, and what the test checks after each is settled.

## Rules

- One manual clock is installed at a time (asserted in debug builds); the destructor uninstalls, so
  `async::manual_clock clock; clock.install();` at the top of a test covers the test.
- The manual time starts at the steady clock's now at the install, so a point computed before the install is
  still meaningful after it, and a timer armed under the manual clock and not yet due keeps its point after the
  uninstall, which the steady clock reaches later.
- The manual clock serves the timers of this module: [sleep](sleep.md), [sleep_until](sleep_until.md),
  [after](after.md), [at](at.md), [tick](tick.md), [timeout](timeout.md), a
  [stop_source](stop_source.md)'s deadline, the races of [with_timeout](with_timeout.md), and a thread's
  `async::sleep(d).wait()` and `async::sleep_until(t).wait()`. `sgcl::this_thread::sleep_for` is the operating
  system's and sleeps for real; the reactor's waits are the kernel's.
- An advance returns when the tasks are settled: a task that waits by spinning, or that holds a worker in a
  blocking call, holds the advance; a task waiting on a channel, a timer, another task or a stop token holds
  nothing. It is called from a thread that is not a worker (the test's), since it waits for the workers to be
  idle.
- The state of the manual clock is the library's, one for the process (a flag and a time), read by every thread;
  the object is a handle on it. Not copyable, not movable.
- Written unqualified under `using namespace sgcl`, the name `clock` collides with `::clock` of `<ctime>`: the
  programs write `sgcl::clock::now()`, and a variable named `clock` hides both.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](manual_clock/manual_clock.md) | constructs a clock, not yet installed |
| `(destructor)` | uninstalls the clock when it is installed |

#### Observers

| Function | Description |
|---|---|
| [installed](manual_clock/installed.md) | checks whether this clock is the module's time |
| [now](manual_clock/now.md) | the manual time |

#### Modifiers

| Function | Description |
|---|---|
| [install](manual_clock/install.md) | makes this clock the module's time, from the steady clock's now |
| [uninstall](manual_clock/uninstall.md) | makes the steady clock the time again |
| [advance](manual_clock/advance.md) | moves the time forward by a span, firing the timers due |
| [advance_to](manual_clock/advance_to.md) | moves the time forward to a point, firing the timers due |

## Complexity

A read of the time, installed or not, is one relaxed load of the flag, and of the time when it is set: the only
cost of the manual clock on the production path. An advance costs the timers it fires and the waits for the
workers to settle.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

// A request that gives up after thirty seconds, and a heartbeat every ten:
// the test of both, where thirty seconds are three advances
async::task<string> request(async::channel<int> reply) {
    string result = "no reply";
    co_await async::select(
        reply.on_receive([&](int) { result = "replied"; }),
        async::timeout(30s, [&] { result = "timed out"; })
    );
    co_return result;
}

int main() {
    async::manual_clock clock;
    clock.install();
    time_point wall = std::chrono::steady_clock::now();
    async::channel<int> reply;
    auto r = async::spawn(request(reply));
    async::channel<void> heartbeat = async::tick(10s);
    int beats = 0;
    for (int i : range(3)) {
        clock.advance(10s);  // the tick; the third time, the timeout too
        beats += heartbeat.try_receive();
    }
    heartbeat.close();
    println("{} heartbeats, the request {}", beats, r.wait());
    println("{}", std::chrono::steady_clock::now() - wall < 1s);
}
```

Output:

```text
3 heartbeats, the request timed out
true
```

## See also

- [clock](../core/clock.md): `clock::now()` and `time_point`, the core's, which the manual clock moves
- [sleep](sleep.md), [sleep_until](sleep_until.md), [after](after.md), [at](at.md), [tick](tick.md),
  [timeout](timeout.md): the timers it serves
- [stop_source::stop_after](stop_source/stop_after.md): a deadline through the clock
- [with_timeout](with_timeout.md): a race against the time, tested the same way
