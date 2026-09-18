# Sleep, SleepUntil, After, At, Tick, Timeout

```cpp
#include "sgcl/Sgcl/Async/Time.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    using Duration = std::chrono::steady_clock::duration;
    using TimePoint = std::chrono::steady_clock::time_point;

    class Sleep;                                              // co_await Sleep(d): the task suspended for d; Sleep(d).Wait() for a thread
    class SleepUntil;                                         // co_await SleepUntil(t): until a point; SleepUntil(t).Wait() for a thread
    Ptr<Channel<void>> After(Duration d);                     // one signal after d, then closed
    Ptr<Channel<void>> At(TimePoint t);                       // one signal at t, then closed
    Ptr<Channel<void>> Tick(Duration d);                      // a signal every d, until closed
    Ptr<Channel<void>> Tick(Duration d, TimePoint first);     // the first at a point, then every d
    template<class F> auto Timeout(Duration d, F f);          // a case of a Select, served after d
    template<class F> auto Timeout(TimePoint t, F f);         // served at t
}
```

The same in the `sgcl` interface: [sleep, sleep_until, after, at, tick, timeout](../../async/timer.md).

Time, the way Go has it. `co_await Sleep(d)` suspends a task for `d` and holds no thread meanwhile: the task is a frame on the managed heap and a timer, and a worker runs it when the time comes; `co_await SleepUntil(t)` the same until a point. `After(d)` is a channel that gets one signal after `d` and is closed then, for whoever wants to wait for a moment as for anything else: `After(1s)->Receive()`, or a case of a select; `At(t)` the same at a point, at once for one that has passed. `Tick(d)` is a channel that gets a signal every `d`: a loop that does something at a fixed rate receives on it; a tick nobody has taken yet is dropped rather than queued (the channel holds one), so a slow loop sees fewer ticks, not a backlog; closing the channel ends the ticks; `Tick(d, first)` puts the first tick at a point (the next whole second, say) and the rest every `d` after it. `Timeout(d, f)` is `After(d)` as a case of a [Select](Select.md), with `f` as the body: the way a wait is bounded; `Timeout(t, f)` is `At(t)` as one, a deadline shared by several selects. A thread sleeps with `Sleep(d).Wait()` or `SleepUntil(t).Wait()`, which block on the timer's channel.

A point is a `TimePoint` of the module's [Clock](Clock.md), `Clock::Now()`: the steady clock's time, unless a test has installed a `ManualClock`, whose time moves only when the test advances it, and then every timer here, and every thread's `Wait()`, goes by the test's time: a sleep of thirty seconds completes on `Advance(30s)`, in microseconds.

Under them one thread with a heap of timers, asleep until the earliest is due and woken when an earlier one is added; it starts with the first timer and stops with the scheduler (`Scheduler::Stop()`, or the end of the program). A timer is a managed object held by a `RootPtr` in the heap: the frame it will resume or the channel it will signal lives while the timer does, and nothing else holds them for it. Firing a timer is a push on the scheduler or a `TrySend` on the channel; the thread never runs a body of the program.

## Rules

- A `co_await` of a `Sleep` or a `SleepUntil` is for a task ([Task](Task.md)); a thread sleeps with `Sleep(d).Wait()` or `SleepUntil(t).Wait()`, through the clock, or with `ThisThread::SleepFor`, the operating system's, which the manual clock does not serve.
- A point is of `Clock`, which is the steady clock's `time_point` (`TimePoint`): a point of the system clock (a calendar time) is converted by the program, `Clock::Now() + (when - system_clock::now())`.
- The resolution is the steady clock's and the thread's wake-up: a timer fires at its time or a little after, never before.
- `After` cannot be cancelled: the channel is signalled and closed at its time whether or not anyone receives; the channel and the timer are garbage after that. A `Tick` ends when its channel is closed, by the program; the timer sees the close at its next tick and lets go.
- The channel of `After` and `Tick` is a managed object (`Ptr<Channel<void>>`): it lives where a `Ptr` may, and as long as something holds it, the timer included.
- The timer thread is a thread of the program to the collector, like any other. It is woken by an arming only when the new timer is the earliest (a later one changes nothing it waits for). A timer armed while `Scheduler::Stop()` is joining the thread stays in the heap, as one not yet due does, and fires once the next arming starts the thread again. A `Timeout` case gone before its time (its select served by another case) cancels its timer: the channel closed, the timer swept out of the heap once the cancelled are half of it, so a select with a long timeout in a loop keeps a bounded heap (a timer per iteration until the deadline before: 0.75 KB each, 300 MB for 400 k iterations, measured).

## Members

### Sleep, SleepUntil

```cpp
explicit Sleep(Duration d) noexcept;          // an awaitable: co_await Sleep(d)
explicit SleepUntil(TimePoint t) noexcept;    // co_await SleepUntil(t)
void Wait() const;                            // both: the thread blocked instead, through the clock
```

Suspends the task for `d`, or until `t`; a `d` of zero or less, or a `t` that has passed, does not suspend.

### After, At, Tick

```cpp
Ptr<Channel<void>> After(Duration d);
Ptr<Channel<void>> At(TimePoint t);
Ptr<Channel<void>> Tick(Duration d);
Ptr<Channel<void>> Tick(Duration d, TimePoint first);
```

### Timeout

```cpp
template<class F> auto Timeout(Duration d, F f);    // f(): the time came before any other case
template<class F> auto Timeout(TimePoint t, F f);   // the same at a point
```

```cpp
Task t = Spawn([]() -> Task<> {
    co_await Sleep(100ms);                                       // no thread held
}());
Ptr later = After(50ms);
later->Receive();                                                // true, after 50 ms; false from then on
Channel<int> data;
Select(
    data.OnReceive([](int v) { /* came in time */ }),
    Timeout(1s, [] { /* did not */ })
);
Ptr every = Tick(10ms);
for (int i = 0; i < 3 && every->Receive(); ++i) { /* at 10, 20, 30 ms */ }
every->Close();                                                  // no more ticks
TimePoint deadline = Clock::Now() + 50ms;                        // a point: the same for a task and a thread
Task u = Spawn([](TimePoint deadline) -> Task<> {
    co_await SleepUntil(deadline);
}(deadline));
SleepUntil(deadline).Wait();                                     // this thread, until the same point
At(deadline)->Receive();                                         // true at once: the point has passed
Ptr aligned = Tick(1s, std::chrono::ceil<std::chrono::seconds>(Clock::Now()));   // on the whole seconds
aligned->Close();
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A poller: every 10 ms it takes what came on the channel, and it gives
// up on a request that took too long. Nothing here holds a thread while
// it waits: the sleeps, the ticks and the timeouts are timers, the task
// a frame on the managed heap.
Task<int> Poll(Channel<int>& data, Channel<void>& stop) {
    Ptr every = Tick(10ms);
    int seen = 0, ticks = 0, late = 0;
    bool running = true;
    while (running) {
        co_await AsyncSelect(
            every->OnReceive([&] { ++ticks; }),
            data.OnReceive([&](int) { ++seen; }),
            stop.OnReceive([&] { running = false; })
        );
    }
    every->Close();
    // a request with a deadline
    Channel<int> reply;
    co_await AsyncSelect(
        reply.OnReceive([&](int) {}),
        Timeout(20ms, [&] { ++late; })
    );
    co_return seen + late * 1000;
}

int main() {
    Channel<int> data(8);
    Channel<void> stop;
    Task p = Spawn(Poll(data, stop));
    for (int i : Range(5)) {
        data.Send(i);
        ThisThread::SleepFor(15ms);
    }
    stop.Close();
    int result = p.Join();
    std::cout << result % 1000 << " seen, " << result / 1000 << " late\n";   // 5 seen, 1 late
    return result == 1005 ? 0 : 1;
}
```

The output:

```
5 seen, 1 late
```

## See also

- [Clock](Clock.md): the clock the points are of, and the manual clock of a test; [Select](Select.md): what `Timeout` is a case of; [Channel](Channel.md): what `After`, `At` and `Tick` are; [Scheduler](Scheduler.md): what runs a task after its sleep
- `tests/Sgcl/sgcl.cpp` and `tests/Sgcl/time_and_signal.cpp`: the behaviour above, checked.
