# sgcl::sleep, sgcl::sleep_until, sgcl::after, sgcl::at, sgcl::tick, sgcl::timeout

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    using duration = std::chrono::steady_clock::duration;
    using time_point = std::chrono::steady_clock::time_point;

    class sleep;                                              // co_await sleep(d): the task suspended for d; sleep(d).wait() for a thread
    class sleep_until;                                        // co_await sleep_until(t): until a point; sleep_until(t).wait() for a thread
    tracked_ptr<channel<void>> after(duration d);             // one signal after d, then closed
    tracked_ptr<channel<void>> at(time_point t);              // one signal at t, then closed
    tracked_ptr<channel<void>> tick(duration d);              // a signal every d, until closed
    tracked_ptr<channel<void>> tick(duration d, time_point first);   // the first at a point, then every d
    template<class F> auto timeout(duration d, F f);          // a case of a select, served after d
    template<class F> auto timeout(time_point t, F f);        // served at t
}
```

Time, the way Go has it. `co_await sleep(d)` suspends a task for `d` and holds no thread meanwhile: the task is a frame on the managed heap and a timer, and a worker runs it when the time comes; `co_await sleep_until(t)` the same until a point. `after(d)` is a channel that gets one signal after `d` and is closed then, for whoever wants to wait for a moment as for anything else: `after(1s)->receive()`, or a case of a select; `at(t)` the same at a point, at once for one that has passed. `tick(d)` is a channel that gets a signal every `d`: a loop that does something at a fixed rate receives on it; a tick nobody has taken yet is dropped rather than queued (the channel holds one), so a slow loop sees fewer ticks, not a backlog; closing the channel ends the ticks; `tick(d, first)` puts the first tick at a point (the next whole second, say) and the rest every `d` after it. `timeout(d, f)` is `after(d)` as a case of a [select](select.md), with `f` as the body: the way a wait is bounded; `timeout(t, f)` is `at(t)` as one, a deadline shared by several selects. A thread sleeps with `sleep(d).wait()` or `sleep_until(t).wait()`, which block on the timer's channel.

A point is a `time_point` of the module's [clock](clock.md), `sgcl::clock::now()`: the steady clock's time, unless a test has installed a manual clock, whose time moves only when the test advances it, and then every timer here, and every thread's `wait()`, goes by the test's time: a sleep of thirty seconds completes on `advance(30s)`, in microseconds.

Under them one thread with a heap of timers, asleep until the earliest is due and woken when an earlier one is added; it starts with the first timer and stops with the scheduler (`scheduler::stop()`, or the end of the program). A timer is a managed object held by a `root_ptr` in the heap: the frame it will resume or the channel it will signal lives while the timer does, and nothing else holds them for it. Firing a timer is a push on the scheduler or a `try_send` on the channel; the thread never runs a body of the program.

## Rules

- A `co_await` of a `sleep` or a `sleep_until` is for a coroutine with a managed frame ([coroutine](coroutine.md)); a thread sleeps with `sleep(d).wait()` or `sleep_until(t).wait()`, through the clock, or with `sgcl::this_thread::sleep_for`, the operating system's, which the manual clock does not serve.
- A point is of `sgcl::clock`, which is the steady clock's `time_point` (`sgcl::time_point`): a point of the system clock (a calendar time) is converted by the program, `sgcl::clock::now() + (when - system_clock::now())`.
- The resolution is the steady clock's and the thread's wake-up: a timer fires at its time or a little after, never before.
- `after` cannot be cancelled: the channel is signalled and closed at its time whether or not anyone receives; the channel and the timer are garbage after that. A `tick` ends when its channel is closed, by the program; the timer sees the close at its next tick and lets go.
- The channel of `after` and `tick` is a managed object (`tracked_ptr<channel<void>>`): it lives where a `tracked_ptr` may, and as long as something holds it, the timer included.
- The timer thread is a thread of the program to the collector, like any other. It is woken by an arming only when the new timer is the earliest (a later one changes nothing it waits for). A timer armed while `scheduler::stop()` is joining the thread stays in the heap, as one not yet due does, and fires once the next arming starts the thread again. A `timeout` case gone before its time (its select served by another case) cancels its timer: the channel closed, the timer swept out of the heap once the cancelled are half of it, so a select with a long timeout in a loop keeps a bounded heap (a timer per iteration until the deadline before: 0.75 KB each, 300 MB for 400 k iterations, measured).

## Members

### sleep, sleep_until

```cpp
explicit sleep(duration d) noexcept;          // an awaitable: co_await sgcl::sleep(d)
explicit sleep_until(time_point t) noexcept;  // co_await sleep_until(t)
void wait() const;                            // both: the thread blocked instead, through the clock
```

Suspends the task for `d`, or until `t`; a `d` of zero or less, or a `t` that has passed, does not suspend.

### after, at, tick

```cpp
tracked_ptr<channel<void>> after(duration d);
tracked_ptr<channel<void>> at(time_point t);
tracked_ptr<channel<void>> tick(duration d);
tracked_ptr<channel<void>> tick(duration d, time_point first);
```

### timeout

```cpp
template<class F> auto timeout(duration d, F f);     // f(): the time came before any other case
template<class F> auto timeout(time_point t, F f);   // the same at a point
```

```cpp
auto t = spawn([]() -> task<> {
    co_await sgcl::sleep(100ms);                                 // no thread held
}());
auto later = after(50ms);
later->receive();                                                // true, after 50 ms; false from then on
channel<int> data;
select(
    data.on_receive([](int v) { /* came in time */ }),
    timeout(1s, [] { /* did not */ })
);
auto every = tick(10ms);
for (int i = 0; i < 3 && every->receive(); ++i) { /* at 10, 20, 30 ms */ }
every->close();                                                  // no more ticks
auto deadline = clock::now() + 50ms;                       // a point: the same for a task and a thread
auto u = spawn([](time_point deadline) -> task<> {
    co_await sleep_until(deadline);
}(deadline));
sleep_until(deadline).wait();                              // this thread, until the same point
at(deadline)->receive();                                   // true at once: the point has passed
auto aligned = tick(1s, std::chrono::ceil<std::chrono::seconds>(clock::now()));   // on the whole seconds
aligned->close();
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

using namespace std::chrono_literals;

// A poller: every 10 ms it takes what came on the channel, and it gives
// up on a request that took too long. Nothing here holds a thread while
// it waits: the sleeps, the ticks and the timeouts are timers, the task
// a frame on the managed heap.
task<int> poll(channel<int>& data, channel<void>& stop) {
    auto every = tick(10ms);
    int seen = 0, ticks = 0, late = 0;
    bool running = true;
    while (running) {
        co_await async_select(
            every->on_receive([&] { ++ticks; }),
            data.on_receive([&](int) { ++seen; }),
            stop.on_receive([&] { running = false; })
        );
    }
    every->close();
    // a request with a deadline
    channel<int> reply;
    co_await async_select(
        reply.on_receive([&](int) {}),
        timeout(20ms, [&] { ++late; })
    );
    co_return seen + late * 1000;
}

int main() {
    channel<int> data(8);
    channel<void> stop;
    auto p = spawn(poll(data, stop));
    for (int i : range(5)) {
        data.send(i);
        this_thread::sleep_for(15ms);
    }
    stop.close();
    int result = p.join();
    std::cout << result % 1000 << " seen, " << result / 1000 << " late\n";   // 5 seen, 1 late
    return result == 1005 ? 0 : 1;
}
```

The output:

```
5 seen, 1 late
```

## See also

- [clock](clock.md): the clock the points are of, and the manual clock of a test; [select](select.md): what `timeout` is a case of; [channel](channel.md): what `after`, `at` and `tick` are; [scheduler](scheduler.md): what runs a task after its sleep
- `tests/async/timer.cpp` and `tests/async/clock.cpp`: every behaviour above, checked.
