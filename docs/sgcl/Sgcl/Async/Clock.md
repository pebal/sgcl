# Clock, ManualClock

```cpp
#include "sgcl/Sgcl/Async/Time.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    struct Clock {                                             // the module's clock
        static TimePoint Now() noexcept;                       // the steady clock's now, or the manual clock's while one is installed
    };

    class ManualClock {                                        // the clock of a test: time moves only by Advance
    public:
        ManualClock();
        ~ManualClock();                                        // uninstalls
        void Install();                                        // the module's time this clock's, from the steady clock's now
        void Uninstall();                                      // the steady clock the time again
        bool IsInstalled() const noexcept;
        TimePoint Now() const noexcept;
        void Advance(Duration d);                              // the time forward by d: every timer due fired, the tasks woken run to their next waits
        void AdvanceTo(TimePoint t);
        sgcl::manual_clock& Inner() noexcept;
    };
}
```

The same in the `sgcl` interface: [clock, manual_clock](../../async/clock.md).

The module reads the time in one place, `Clock::Now()`: the deadline a `Sleep`, an `After`, a `Tick`, a `Timeout` or a `StopAfter` computes, the now the timer thread compares them with. It is the steady clock's time unless a test has installed a `ManualClock`, and then it is the manual clock's, which stands still until the test moves it: `Advance(30s)` moves it thirty seconds forward, and every timer due by then fires, in the order of its deadlines, with no waiting at all. This is what tokio's `time::pause()` and `advance()` and Kotlin's `runTest` give: a test of a thirty-second timeout, of a retry every minute, of a deadline at midnight, takes microseconds and is deterministic, because the time is not the machine's but the test's.

An `Advance` does three things, in order: it waits until every task has reached its next wait (no task ready, none running, every worker asleep), so that a task spawned a moment ago has armed its timer; it moves the time and wakes the timer thread, which fires every timer now due and reports back when nothing is; and it waits again until the tasks those timers woke have reached their next waits, so that a task that sleeps in a loop has armed its next sleep when the advance returns, and the next advance finds it. A chain of `Advance(1s)` therefore runs a task through its loop one lap per call, and what the test checks after each is settled. A tick whose several periods fall into one advance fires once per period on the timer thread, but the channel holds one signal, so a receiver that was not receiving in between sees one, as a slow receiver does in real time.

The production path pays one relaxed load of the flag per read of the time and nothing else; the numbers are on the [sgcl page](../../async/clock.md).

## Rules

- One manual clock installed at a time (asserted); the destructor uninstalls, so `ManualClock clock; clock.Install();` at the top of a test covers the test.
- The manual time starts at the steady clock's now at the install, so a point computed before the install is still meaningful after it, and a timer armed under the manual clock and not yet due keeps its point after the uninstall, which the steady clock reaches later.
- `Advance` returns when the tasks are settled: a task that waits by spinning, or that holds a worker in a blocking call, holds the advance; a task waiting on a channel, a timer, another task, a stop token, holds nothing. It is called from a thread that is not a worker (the test's), since it waits for the workers to be idle.
- The manual clock serves the timers of this module: a `Sleep`, `SleepUntil`, `After`, `At`, `Tick`, `Timeout`, `StopAfter`, and a thread's `Sleep(d).Wait()` and `SleepUntil(t).Wait()`. `ThisThread::SleepFor` is the operating system's and sleeps for real; the reactor's waits are the kernel's.
- The state of the manual clock is the module's (a flag and a time), read by every thread; the object is a handle on it.

## Members

### Clock::Now

```cpp
static TimePoint Now() noexcept;
```

### ManualClock

```cpp
void Install();                     // one at a time; the time from the steady clock's now
void Uninstall();                   // the destructor does it
bool IsInstalled() const noexcept;
TimePoint Now() const noexcept;     // the manual time
void Advance(Duration d);           // never backwards
void AdvanceTo(TimePoint t);
```

```cpp
ManualClock clock;
clock.Install();
Task t = Spawn([]() -> Task<int> {
    co_await Sleep(30s);                         // thirty seconds of the manual clock
    co_return 1;
}());
clock.Advance(29s);                              // not yet
clock.Advance(1s);                               // the task ran to its end before this returned
int one = t.Join();                              // 1, microseconds after the Spawn
Ptr every = Tick(1s);
clock.Advance(1s);
bool ticked = every->TryReceive();               // true: one tick per period advanced
every->Close();
clock.Uninstall();                               // the steady clock again
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A request that gives up after thirty seconds, and a heartbeat every
// ten: the test of both under a manual clock, where thirty seconds are
// three advances and no waiting at all
Task<const char*> Request(Channel<int>& reply) {
    const char* result = "no reply";
    co_await AsyncSelect(
        reply.OnReceive([&](int) { result = "replied"; }),
        Timeout(30s, [&] { result = "timed out"; })
    );
    co_return result;
}

int main() {
    ManualClock clock;
    clock.Install();                          // the module's time stops here
    auto wall = std::chrono::steady_clock::now();
    Channel<int> reply;
    Task r = Spawn(Request(reply));
    Ptr heartbeat = Tick(10s);
    int beats = 0;
    for (int i : Range(3)) {
        (void)i;
        clock.Advance(10s);                   // the tick fires; the third time, the timeout too
        if (heartbeat->TryReceive()) {
            ++beats;
        }
    }
    heartbeat->Close();
    auto took = std::chrono::steady_clock::now() - wall;
    std::cout << beats << " heartbeats, the request " << r.Join() << ", in " << (took < 1s ? "under" : "over") << " a second of wall time\n";
    Scheduler::Stop();
}
```

The output:

```
3 heartbeats, the request timed out, in under a second of wall time
```

## See also

- [Time](Time.md): what the clock serves: `Sleep`, `SleepUntil`, `After`, `At`, `Tick`, `Timeout`; [StopToken](StopToken.md): `StopAfter`, a deadline through the clock
- `tests/Sgcl/time_and_signal.cpp`: the behaviour above, checked; `tests/async/clock.cpp`: the whole of it, on the `sgcl` class.
