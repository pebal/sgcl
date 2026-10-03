[sgcl](../README.md) › [async](README.md)

# sgcl::async::at

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    event at(time_point t);
}
```

Returns an [event](event/README.md) set at the point `t` of the module's clock: [after](after.md) by a point, waited for
the same three ways (`co_await`, `wait()`, `on_set(f)` in a [select](select.md)). A point that has passed sets
the event at once, by the timer thread: a wait returns without delay, though `is_set()` read right after the call
may still be `false`. A point of `time_point::max()` is never reached.

The point is of `sgcl::clock`, the steady clock's `time_point`, or the manual clock's time while a test has one
installed; a calendar time is converted by the program, as on [sleep_until](sleep_until.md).

## Parameters

| Parameter | Description |
|---|---|
| `t` | the point at which the event is set, of `sgcl::clock` |

## Return value

The event, set at `t`: a handle, one tracked word ([Handles](README.md#handles)). The state lives as long as
something holds it, the timer included.

## Complexity

Constant: the event's state and a timer on the managed heap, and the timer's push into a heap of timers,
logarithmic in the timers of that heap.

## Exceptions

`std::system_error` when the timer thread cannot be started.

## Notes

The timers due fire in the order of their points: of two events, the one with the earlier point is set first. The
timer thread and what the timers share are on [sleep](sleep.md#notes).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    time_point start = sgcl::clock::now();
    async::event first = async::at(start + 10ms);
    async::event second = async::at(start + 20ms);
    second.wait();
    println("{} {}", first.is_set(), sgcl::clock::now() >= start + 20ms);

    async::at(start).wait();  // passed: returns at once
    println("done");
}
```

Output:

```text
true true
done
```

## See also

- [after](after.md): the same after a while
- [sleep_until](sleep_until.md): a task or a thread waiting until a point
- [timeout](timeout.md): a point as the deadline of a select
- [clock](../core/clock/README.md): the clock the points are of
- [event](event/README.md): what `at` returns
