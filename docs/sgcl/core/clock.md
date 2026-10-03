[sgcl](../README.md) › [core](README.md)

# sgcl::clock

```cpp
#include "sgcl/core/clock.h"   // or "sgcl/core.h"

namespace sgcl {
    using time_point = std::chrono::steady_clock::time_point;

    struct clock;
}
```

`sgcl::clock` is the library's clock: the one place the library reads the time, `clock::now()`. The deadline a
`sleep`, an `after`, a `tick`, a `timeout` or a `stop_after` of [async](../async/README.md#time) computes, the `now` the
timer thread compares them with, the start and the reading of a [stopwatch](../time/stopwatch.md): all of
them are `clock::now()`. It is the steady clock's time unless a test has installed a
[manual_clock](../async/manual_clock.md), and then it is the manual clock's, which stands still until the test
moves it: code that measures or waits is tested in microseconds and deterministically, because the time is not the
machine's but the test's. The manual clock itself steers the timers, so it is async's; the clock, which modules
below async read too, is core's.

The clock is a Clock of the standard library (`rep`, `period`, `duration`, `time_point`, `is_steady`, `now`), so
it serves where one is asked for. A point is `sgcl::time_point`, the steady clock's `std::chrono::time_point`; its
`duration` is the steady clock's, a `std::chrono::duration` as a Clock's must be, which
[sgcl::duration](duration.md) converts to and from: `clock::now() + 30s` and `clock::now() + d` for a `duration d`
are both points, the second saturated at `time_point::max()` rather than wrapped.

The production path pays one relaxed load of the manual clock's flag per read and nothing else: `clock::now()`
costs what `steady_clock::now()` does.

## Rules

- Written unqualified under `using namespace sgcl`, the name `clock` collides with `::clock` of `<ctime>`: write
  `sgcl::clock::now()`.
- The state the manual clock sets (a flag and a time) is the library's, one for the process, read by every
  thread; `now()` may be called from any thread.
- A `clock` has no state of its own and no object of it is needed: its members are static.

## Member types

| Type | Definition |
|---|---|
| `rep` | `time_point::rep`, the steady clock's count |
| `period` | `time_point::period`, the steady clock's unit |
| `duration` | `time_point::duration`, the steady clock's `std::chrono::duration`; `sgcl::duration` converts to and from it |
| `time_point` | `sgcl::time_point`, the steady clock's `std::chrono::time_point` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `is_steady` | `true` | the time never goes back: the steady clock's, or the manual clock's, which moves only forward; `static constexpr bool` |

## Member functions

| Function | Description |
|---|---|
| [now](clock/now.md) | the library's time (static) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    time_point deadline = sgcl::clock::now() + std::chrono::milliseconds(50);
    int laps = 0;
    while (sgcl::clock::now() < deadline) {
        ++laps;
    }
    duration late = sgcl::clock::now() - deadline;
    println("{} laps, {} time", (laps > 0 ? "some" : "no"),
            (late < std::chrono::seconds(1) ? "on" : "past"));
}
```

Output:

```text
some laps, on time
```

## See also

- [manual_clock](../async/manual_clock.md): the clock of a test, which stops the library's time and moves it by
  `advance`
- [duration](duration.md): the span of time the clock's points are moved by
- [sleep, after, tick, timeout](../async/README.md#time): what waits on the clock
- [time](../time/README.md): the stopwatch that reads it
- `tests/async/clock.cpp`: the clock under a manual clock, checked
