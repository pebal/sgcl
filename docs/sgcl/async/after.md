[sgcl](../README.md) › [async](README.md)

# sgcl::async::after

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    event after(duration d);
}
```

Returns an [event](event/README.md) set after `d`, for whoever wants to wait for a moment as for anything else:
`co_await async::after(1s)` in a task, `async::after(1s).wait()` on a thread, `async::after(1s).on_set(f)` as a
case of a [select](select.md). Go's `time.After` gives a channel that one receiver takes the time from; this is
an event, so any number of tasks and threads wait for the same moment, and a wait that starts after it does not
wait.

The event is set at its time whether or not anyone waits; the event and the timer are garbage after that. Under it
is a channel of signals that the timer closes. A span too long for the clock saturates, and
`async::after(duration::max())` is never set.

## Parameters

| Parameter | Description |
|---|---|
| `d` | how long until the event is set |

## Return value

The event, set after `d`: a handle, one tracked word, living on a stack, in a task or in a managed object, and in
a global or a std container as a `rooted<async::event>` ([Handles](README.md#handles)). The state lives as long
as something holds it, the timer included.

## Complexity

Constant: the event's state and a timer on the managed heap, and the timer's push into a heap of timers,
logarithmic in the timers of that heap.

## Exceptions

`std::system_error` when the timer thread cannot be started.

## Notes

[timeout](timeout.md) is `after` as a case of a select with a body, which cancels its timer when the select is
served by another case. The timer thread and what the timers share are on [sleep](sleep.md#notes).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<string> waiter(async::event moment) {
    co_await moment;
    co_return "the task";
}

int main() {
    async::event later = async::after(20ms);
    println("{}", later.is_set());
    auto t = async::spawn(waiter(later));
    later.wait();
    println("{}", later.is_set());
    println("{} too", t.wait());
    later.wait();  // set: returns at once
}
```

Output:

```text
false
true
the task too
```

## See also

- [at](at.md): the same at a point of the clock
- [timeout](timeout.md): a moment as a case of a select
- [sleep](sleep.md): a task or a thread waiting for a while
- [event](event/README.md): what `after` returns
