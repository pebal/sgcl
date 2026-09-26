# sgcl::async::manual_clock

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class manual_clock {                                       // the clock of a test: time moves only by advance
    public:
        manual_clock();
        ~manual_clock();                                       // uninstalls
        void install();                                        // the module's time this clock's, from the steady clock's now
        void uninstall();                                      // the steady clock the time again
        bool installed() const noexcept;
        time_point now() const noexcept;
        void advance(duration d);                              // the time forward by d: every timer due fired, the tasks woken run to their next waits
        void advance_to(time_point t);
    };
}
```

The library reads the time in one place, [`clock::now()`](../core/clock.md) (core): the deadline a `sleep`, an `after`, a `tick`, a `timeout` or a `stop_after` computes, the `now` the timer thread compares them with. It is the steady clock's time unless a test has installed a `manual_clock`, and then it is the manual clock's, which stands still until the test moves it: `advance(30s)` moves it thirty seconds forward, and every timer due by then fires, in the order of its deadlines, with no waiting at all. This is what tokio's `time::pause()` and `advance()` and Kotlin's `runTest` give: a test of a thirty-second timeout, of a retry every minute, of a deadline at midnight, takes microseconds and is deterministic, because the time is not the machine's but the test's. The clock is a Clock of the standard library (`rep`, `period`, `duration`, `time_point`, `is_steady`, `now`), so it serves where one is asked for; its `duration` is the steady clock's, a `std::chrono::duration` as a Clock's must be, which [`sgcl::duration`](../core/duration.md), what `advance` and the timers take, converts to and from.

An `advance` does three things, in order: it waits until every task has reached its next wait (no task ready, none running, every worker asleep), so that a task spawned a moment ago has armed its timer; it moves the time and wakes the timer thread, which fires every timer now due and reports back when nothing is; and it waits again until the tasks those timers woke have reached their next waits, so that a task that sleeps in a loop has armed its next sleep when the advance returns, and the next advance finds it. A chain of `advance(1s)` therefore runs a task through its loop one lap per call, and what the test checks after each is settled. A tick whose several periods fall into one advance fires once per period on the timer thread, but the channel holds one signal, so a receiver that was not receiving in between sees one, as a slow receiver does in real time.

The production path pays one relaxed load of the flag per read of the time and nothing else: measured, `clock::now()` at 17 to 18 ns per call as `steady_clock::now()` was, a hundred thousand sleeping tasks fired through the heap in 135 to 145 ms as before, a timer armed in 455 to 465 ns as before (`tests/async/clock.cpp` and a probe of the three, the release build, before and after).

## Rules

- One manual clock installed at a time (asserted); the destructor uninstalls, so `sgcl::async::manual_clock c; c.install();` at the top of a test covers the test. Written unqualified under `using namespace sgcl`, the name `clock` collides with `::clock` of `<ctime>`: write `sgcl::clock::now()`.
- The manual time starts at the steady clock's now at the install, so a point computed before the install is still meaningful after it, and a timer armed under the manual clock and not yet due keeps its point after the uninstall, which the steady clock reaches later.
- `advance` returns when the tasks are settled: a task that waits by spinning, or that holds a worker in a blocking call, holds the advance; a task waiting on a channel, a timer, another task, a stop token, holds nothing. It is called from a thread that is not a worker (the test's), since it waits for the workers to be idle.
- The manual clock serves the timers of this module: a `sleep`, `sleep_until`, `after`, `at`, `tick`, `timeout`, `stop_after`, and a thread's `async::sleep(d).wait()` and `async::sleep_until(t).wait()`. `sgcl::this_thread::sleep_for` is the operating system's and sleeps for real; the reactor's waits are the kernel's.
- The state of the manual clock is the module's (a flag and a time), read by every thread; the object is a handle on it.

## Members

### manual_clock

```cpp
void install();                    // one at a time; the time from the steady clock's now
void uninstall();                  // the destructor does it
bool installed() const noexcept;
time_point now() const noexcept;   // the manual time
void advance(duration d);          // never backwards
void advance_to(time_point t);
```

```cpp
async::manual_clock clock;
clock.install();
auto t = async::spawn([]() -> async::task<int> {
    co_await async::sleep(30s);                   // thirty seconds of the manual clock
    co_return 1;
}());
clock.advance(29s);                              // not yet
clock.advance(1s);                               // the task ran to its end before this returned
int one = t.wait();                              // 1, microseconds after the spawn
auto every = async::tick(1s);
clock.advance(1s);
bool ticked = every->try_receive();              // true: one tick per period advanced
every->close();
clock.uninstall();                               // the steady clock again
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

using namespace std::chrono_literals;

// A request that gives up after thirty seconds, and a heartbeat every
// ten: the test of both under a manual clock, where thirty seconds are
// three advances and no waiting at all
async::task<const char*> request(async::channel<int>& reply) {
    const char* result = "no reply";
    co_await async::select(
        reply.on_receive([&](int) { result = "replied"; }),
        async::timeout(30s, [&] { result = "timed out"; })
    );
    co_return result;
}

int main() {
    async::manual_clock clock;
    clock.install();                          // the module's time stops here
    auto wall = std::chrono::steady_clock::now();
    async::channel<int> reply;
    auto r = async::spawn(request(reply));
    auto heartbeat = async::tick(10s);
    int beats = 0;
    for (int i : range(3)) {
        (void)i;
        clock.advance(10s);                   // the tick fires; the third time, the timeout too
        if (heartbeat->try_receive()) {
            ++beats;
        }
    }
    heartbeat->close();
    auto took = std::chrono::steady_clock::now() - wall;
    std::cout << beats << " heartbeats, the request " << r.wait() << ", in " << (took < 1s ? "under" : "over") << " a second of wall time\n";
    async::scheduler::stop();
}
```

The output:

```
3 heartbeats, the request timed out, in under a second of wall time
```

## See also

- [clock](../core/clock.md): `clock::now()` and `time_point`, core's, which the manual clock moves; [timer](timer.md): what the clock serves: `sleep`, `sleep_until`, `after`, `at`, `tick`, `timeout`; [stop_token](stop_token.md): `stop_after`, a deadline through the clock
- `tests/async/clock.cpp`: every behaviour above, checked, under the thread sanitizer too.
