[sgcl](../README.md) › [async](README.md)

# sgcl::async::tick

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    channel<void> tick(duration d);                      // (1)
    channel<void> tick(duration d, time_point first);    // (2)
}
```

Returns a channel that gets a signal every `d`, Go's `time.Tick`: a loop that does something at a fixed rate
receives on it, a select takes it as a case.

1. The first signal after `d`, then every `d`.
2. The first signal at the point `first` (the next whole second, say), then every `d` after it.

A tick nobody has taken yet is dropped rather than queued: the channel holds one signal, so a slow loop sees fewer
ticks, not a backlog. The ticks go on until the program closes the channel; the timer sees the close at its next
tick and lets go. A tick past the end of time stops at `time_point::max()`, which never fires, so
`async::tick(duration::max())` never ticks. A period of zero or less never ticks either, as Go's `time.Tick`
gives a nil channel for one: no timer is armed, and the channel stays open until the program closes it.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the period; zero or less: no tick |
| `first` | the point of the first tick, of `sgcl::clock` |

## Return value

The channel of the ticks: a handle, one tracked word ([Handles](README.md#handles)), on a stack, in a task or in a
managed object, and in a global or a std container as a `rooted<async::channel<void>>`. Its state lives as long
as something holds it, the timer included, until the channel is closed.

## Complexity

Constant: the channel's state and a timer on the managed heap, pushed into a heap of timers (logarithmic in the
timers of that heap), and pushed again at every tick.

## Exceptions

`std::system_error` when the timer thread cannot be started.

## Notes

A tick is a send that does not wait, made by the timer thread: when the receiver is a task, a push on the
scheduler. Under a [manual_clock](manual_clock.md), an advance that covers several periods fires the timer once
per period, and the channel holds one signal of them, as for a slow receiver in real time. The timer thread and
what the timers share are on [sleep](sleep.md#notes).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    time_point start = sgcl::clock::now();
    async::channel<void> every = async::tick(10ms);
    int ticks = 0;
    while (ticks < 3 && every.receive().wait()) {
        ++ticks;
    }
    every.close();  // no more ticks
    println("{} ticks, {}", ticks, sgcl::clock::now() - start >= 30ms);
}
```

Output:

```text
3 ticks, true
```

A slow receiver and a first tick at a point, under a manual clock:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();

    async::channel<void> every = async::tick(1s);
    clock.advance(3s);  // three ticks, nobody receiving
    bool first = every.try_receive();
    bool second = every.try_receive();
    println("{} {}", first, second);
    every.close();

    async::channel<void> aligned = async::tick(1s, clock.now() + 250ms);
    clock.advance(250ms);
    println("{}", aligned.try_receive());
    clock.advance(999ms);
    println("{}", aligned.try_receive());
    clock.advance(1ms);
    println("{}", aligned.try_receive());
    aligned.close();
}
```

Output:

```text
true false
true
false
true
```

## See also

- [every](every.md): a function called every period, the loop over the ticks written
- [after](after.md): one moment, as an event
- [select](select.md): a tick as a case beside others
- [channel](channel.md): what `tick` returns
- [manual_clock](manual_clock.md): the ticks of a test
