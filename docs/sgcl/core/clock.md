# sgcl::clock, sgcl::time_point

```cpp
#include "sgcl/core/clock.h"   // or "sgcl/core/core.h", "sgcl/sgcl.h"

namespace sgcl {
    using time_point = std::chrono::steady_clock::time_point;

    struct clock {                                             // the library's clock: a Clock of the standard library
        using rep = time_point::rep;
        using period = time_point::period;
        using duration = time_point::duration;                 // a std::chrono::duration, as a Clock's must be
        using time_point = sgcl::time_point;
        static constexpr bool is_steady = true;
        static time_point now() noexcept;                      // the steady clock's now, or the manual clock's while one is installed
    };
}
```

The library reads the time in one place, `clock::now()`: the deadline a `sleep`, an `after`, a `tick`, a `timeout` or a `stop_after` of [async](../async/timer.md) computes, the `now` the timer thread compares them with, the start and the reading of a [stopwatch](../time/README.md#stopwatch). It is the steady clock's time unless a test has installed a [`manual_clock`](../async/manual_clock.md), and then it is the manual clock's, which stands still until the test moves it: code that measures or waits is tested in microseconds and deterministically, because the time is not the machine's but the test's. The manual clock itself steers the timers, so it is async's; the clock, which modules below async read too, is core's.

The clock is a Clock of the standard library (`rep`, `period`, `duration`, `time_point`, `is_steady`, `now`), so it serves where one is asked for. A point is `sgcl::time_point`, the steady clock's `std::chrono::time_point`; its `duration` is the steady clock's, a `std::chrono::duration` as a Clock's must be, which [`sgcl::duration`](duration.md) converts to and from: `clock::now() + 30s` and `clock::now() + d` for a `duration d` are both points, the second saturated at `time_point::max()` rather than wrapped.

The production path pays one relaxed load of the manual clock's flag per read and nothing else: `clock::now()` costs what `steady_clock::now()` does.

## Rules

- Written unqualified under `using namespace sgcl`, the name `clock` collides with `::clock` of `<ctime>`: write `sgcl::clock::now()`.
- The state the manual clock sets (a flag and a time) is the library's, one for the process, read by every thread; `now()` may be called from any thread.

## Members

### clock::now

```cpp
static time_point now() noexcept;
```

The library's time: the steady clock's, or the manual clock's while one is installed.

```cpp
time_point start = sgcl::clock::now();
// ... work
duration took = sgcl::clock::now() - start;   // a std::chrono duration, converted
```

## Example

```cpp
#include "sgcl/core/clock.h"
#include <iostream>

using namespace sgcl;

using namespace std::chrono_literals;

int main() {
    time_point deadline = sgcl::clock::now() + 50ms;
    int laps = 0;
    while (sgcl::clock::now() < deadline) {
        ++laps;
    }
    duration late = sgcl::clock::now() - deadline;
    std::cout << (laps > 0 ? "some" : "no") << " laps, " << (late < 1s ? "on" : "past") << " time\n";
}
```

The output:

```
some laps, on time
```

## See also

- [manual_clock](../async/manual_clock.md): the clock of a test, which stops the library's time and moves it by `advance`
- [duration](duration.md): the span of time the clock's points are moved by; [timer](../async/timer.md): what waits on the clock; [time](../time/README.md): the stopwatch that reads it
- `tests/async/clock.cpp`: the clock under a manual clock, checked.
