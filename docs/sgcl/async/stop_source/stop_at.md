[sgcl](../../README.md) › [async](../README.md) › [stop_source](../stop_source.md)

# sgcl::async::stop_source::stop_at

```cpp
void stop_at(time_point when);
```

Requests the stop at the point `when` of the module's clock, by a timer: [stop_after](stop_after.md) by a point,
Go's `context.WithDeadline`. The earliest deadline armed stands, a later one arms nothing, the stop cancels the
timer, and a point of `time_point::max()` arms nothing, as on [stop_after](stop_after.md). A point that has passed
stops the source at once, by the timer thread.

## Parameters

| Parameter | Description |
|---|---|
| `when` | the point of the stop, of `sgcl::clock` |

## Return value

None.

## Complexity

Constant: a deadline and a timer on the managed heap, and the timer's push into a heap of timers, logarithmic in
the timers of that heap.

## Exceptions

`std::system_error` when the timer thread cannot be started, or when the stop wakes a task and the scheduler's
workers cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<string> work(async::stop_token token) {
    co_await token.stopped();
    co_return "stopped at the deadline";
}

int main() {
    async::manual_clock clock;
    clock.install();
    time_point deadline = clock.now() + 1h;
    async::stop_source first, second;
    first.stop_at(deadline);
    second.stop_at(deadline);  // one deadline for both
    auto a = async::spawn(work(first.token()));
    auto b = async::spawn(work(second.token()));
    clock.advance_to(deadline);
    println("{}", a.wait());
    println("{}", b.wait());
}
```

Output:

```text
stopped at the deadline
stopped at the deadline
```

## See also

- [stop_after](stop_after.md): after a span
- [with_deadline](../with_deadline.md): a task raced against a point
- [sgcl::async::stop_source](../stop_source.md)
