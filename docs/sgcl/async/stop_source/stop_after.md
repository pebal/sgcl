[sgcl](../../README.md) › [async](../README.md) › [stop_source](../stop_source.md)

# sgcl::async::stop_source::stop_after

```cpp
void stop_after(duration d);
```

Requests the stop after `d`, by a timer: a deadline, Go's `context.WithTimeout`. When the time comes, the timer
thread makes the same stop as [request_stop](request_stop.md), the channel closed and the children stopped in one
step, so the stop is requested before anyone woken by it looks.

The source has one deadline: the earliest armed stands. A later deadline beside an earlier one arms nothing; an
earlier one replaces it, and the replaced timer is cancelled. The stop, by hand or by the parent's, cancels the
timer too; a source stopped already arms nothing. A span that reaches the end of time (`duration::max()`) arms
nothing either: the point saturates at `time_point::max()`, which never comes.

## Parameters

| Parameter | Description |
|---|---|
| `d` | how long until the stop |

## Return value

None.

## Complexity

Constant: a deadline and a timer on the managed heap, and the timer's push into a heap of timers, logarithmic in
the timers of that heap.

## Exceptions

`std::system_error` when the timer thread cannot be started, or when the stop wakes a task and the scheduler's
workers cannot be started.

## Notes

The timer holds the deadline and not the source's state, which a cancel lets go of: a source stopped by hand is
not kept alive by its cancelled timer while it waits in its heap for a sweep, as Go's `cancel()` stops the timer
of a `WithDeadline`. The deadline goes by the module's clock, so a [manual_clock](../manual_clock.md) serves it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    async::stop_source source;
    source.stop_after(30s);
    source.stop_after(1min);  // later: arms nothing
    clock.advance(29s);
    println("{}", source.stop_requested());
    clock.advance(1s);
    println("{}", source.stop_requested());
}
```

Output:

```text
false
true
```

## See also

- [stop_at](stop_at.md): at a point of the clock
- [request_stop](request_stop.md): the stop at once
- [with_timeout](../with_timeout.md): a task raced against a span
- [sgcl::async::stop_source](../stop_source.md)
