[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::event

```cpp
#include "sgcl/async/event.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class event {
    public:
        class wait_op;

        friend bool operator==(const event& a, const event& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::event` is set once and waited for by any number of tasks and threads, and a wait after the set does
not wait. Under it is a channel of signals that [set](set.md) closes: Go's idiom of a channel closed to tell
every receiver at once that something happened, `close(done)`. It is one of the family the [mutex](../mutex/README.md) heads,
each a channel of signals under the name of what it does, and is waited for as a task is: `e.wait()` on a thread,
`co_await e` in a task, which holds no thread while it waits, and `e.on_set(f)` as a case of a [select](../select.md).
A [promise](../promise/README.md) is an event with a value.

What the reactor and the timers give is an event too: [readable](../readable.md), [writable](../writable.md),
[exited](../exited.md), [after](../after.md) and [at](../at.md) return one, set when the moment comes or when the wait is
ended with nothing, by a cancel or a stop. A wait woken by such an event looks at its source again.

## Rules

- An event is a handle: one word, a tracked word to its channel, which copies share;
  [operator==](operator_cmp.md) says whether two are the same event. It is made unset by its constructor, and
  there is no empty event ([README: Handles](../README.md#handles)).
- Set once: there is no reset, and a second [set](set.md) does nothing.
- An event happens once, and a wait ends when it has happened: after [wait](wait.md), `co_await` or a select's
  [on_set](on_set.md) case, [is_set](is_set.md) is `true`, for the events of the reactor and the timers
  too.
- `wait()` is a thread's: a task on a worker writes `co_await e` ([README: The rules](../README.md#the-rules), 1).

## Member types

| Type | Definition |
|---|---|
| `wait_op` | the awaiter of `co_await e` ([wait, operator co_await](wait.md)) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](event.md) | constructs an unset event, or a handle of the same event |
| `(destructor)` | lets go of the handle; the channel is the collector's once no handle holds it |
| [operator=](operator_assign.md) | makes the handle one of another event |

#### Notification

| Function | Description |
|---|---|
| [set](set.md) | sets the event, waking every waiter |

#### Observers

| Function | Description |
|---|---|
| [is_set](is_set.md) | checks whether the event is set |

#### Waiting

| Function | Description |
|---|---|
| [wait, operator co_await](wait.md) | waits for the set, on a thread or in a task |
| [on_set](on_set.md) | a case of a select: a call once the event is set |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same event |

## Complexity

A set is the close of the channel, linear in the waiters it wakes. A wait on an event that is set is a look at the
channel; one that waits registers a waiter on the channel's list.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> runner(async::event start, int id) {  // by value: a copy is the same event
    co_await start;  // all start together
    co_return id * 10;
}

int main() {
    async::event start;
    vector<async::task<int>> runners;
    for (int id : range(3)) {
        runners.push_back(async::spawn(runner(start, id)));
    }
    println("set: {}", start.is_set());
    start.set();
    for (auto& r : runners) {
        println("{}", r.wait());
    }
    println("set: {}", start.is_set());
}
```

Output:

```text
set: false
0
10
20
set: true
```

## See also

- [promise](../promise/README.md): an event with a value
- [wait_group](../wait_group/README.md): the wait for a count to reach zero
- [mutex](../mutex/README.md), [semaphore](../semaphore/README.md), [once](../once/README.md): the rest of the family
- [channel](../channel/README.md): what it is made of; [select](../select.md): its case
- [README: Handles](../README.md#handles)
