[sgcl](../README.md) › [async](README.md)

# sgcl::async::timeout

```cpp
#include "sgcl/async/timer.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class F>
    class timeout_case {
    public:
        timeout_case(timeout_case&& o) noexcept;
        timeout_case(const timeout_case&) = delete;
        ~timeout_case();
    };

    template<class F>
    auto timeout(duration d, F f);                     // (1)
    template<class F>
    auto timeout(time_point t, F f);                   // (2)
}
```

Returns a case of a [select](select.md) served when the time comes, with `f` as its body: the way a wait is
bounded, Go's `case <-time.After(d):`. The select takes the first case ready; when no other is ready by then, it
takes this one and runs `f()`.

1. Served after `d`.
2. Served at the point `t` of the module's clock: a deadline computed once and shared by several selects, a loop
   of them included.

The case is a `timeout_case<F>`, made by `timeout` alone and movable only: it is [after](after.md) as a case, an
event's channel closed by a timer, kept by the case. A case gone before its time (its select served by another
case, the usual end of a select with a timeout in a loop) cancels its timer: the channel closed, the timer swept
out of the heap of timers once the cancelled are half of it, so that a select with a long timeout in a loop keeps
a bounded heap. That close throws nothing (the destructor is noexcept): it wakes nobody, since the select the case
was in is over by then. A span too long for the clock saturates, and a timeout of `duration::max()` is never served.

## Parameters

| Parameter | Description |
|---|---|
| `d` | how long the select may wait for the other cases |
| `t` | the point until which it may wait, of `sgcl::clock` |
| `f` | the body: called with no arguments when the case is served |

## Return value

The case, a `timeout_case<F>`, to give to `select`: `co_await async::select(..., async::timeout(d, f))` in a
task, `async::select(..., async::timeout(d, f)).wait()` on a thread. The select gives the index of the case it
served, as for any case.

## Complexity

Constant: the channel's state and a timer on the managed heap, and the timer's push into a heap of timers,
logarithmic in the timers of that heap.

## Exceptions

What the move constructor of `F` throws, and `std::system_error` when the timer thread cannot be started. What
`f` throws when the case is served goes out of the select.

## Notes

The timer is armed when `timeout` is called, not when the select starts waiting: the time runs from the call.
The timer thread and what the timers share are on [sleep](sleep.md#notes). For a timeout on a whole task, a
[task](task/README.md) rather than a wait, [with_timeout](with_timeout.md) races it against the time.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::channel<int> replies;
    size_t which = async::select(
        replies.on_receive([](int v) { println("reply {}", v); }),
        async::timeout(20ms, [] { println("no reply in 20 ms"); })
    ).wait();
    println("case {}", which);
}
```

Output:

```text
no reply in 20 ms
case 1
```

A deadline shared by the selects of a loop:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> collect(async::channel<int> results, time_point deadline) {
    int sum = 0;
    bool open = true;
    while (open) {
        co_await async::select(
            results.on_receive([&](int v) { sum += v; }),
            async::timeout(deadline, [&] { open = false; })
        );
    }
    co_return sum;
}

int main() {
    async::channel<int> results(8);
    for (int v : {1, 2, 3}) {
        results.send(v).wait();
    }
    println("{}", collect(results, sgcl::clock::now() + 20ms).wait());
}
```

Output:

```text
6
```

## See also

- [select](select.md): what `timeout` is a case of
- [after](after.md), [at](at.md): the same moment as an event
- [with_timeout](with_timeout.md), [with_deadline](with_deadline.md): a timeout on a task
- [stop_token::on_stop](stop_token/on_stop.md): the stop as a case beside it
