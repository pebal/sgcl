[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::condition_variable

```cpp
#include "sgcl/async/condition_variable.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class condition_variable;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::condition_variable` is a wait under the module's [mutex](../mutex/README.md) for a notify, for tasks and
threads alike: Go's `sync.Cond`, and `std::condition_variable_any` over that mutex. A [wait](wait.md)
lets go of the mutex, waits for a notify and takes the mutex back; a task waiting holds no thread, and takes the
mutex back with a `co_await` too.

Under it is a queue of waiters, each a channel of one signal: [notify_one](notify_one.md) takes
the first and sends it the signal, [notify_all](notify_all.md) every one. A waiter is on the
queue before the mutex is let go of, so a notify made under the mutex after the wait began reaches it, and a signal
sent before the waiter reaches its receive is kept for it in its channel: no wakeup is lost between the unlock and
the wait. The one that is lost is the one a condition variable loses by contract: a notify before the wait began
finds no waiter, so the waiter checks its condition under the mutex before it waits, as Go's and the standard's
must; the forms of `wait` with a predicate do it.

## Rules

- A condition variable is an object, not a handle: it is neither copied nor moved. Tasks reach it through the object
  that holds it.
- It waits with the module's `mutex`: a task with the [guard](../mutex-guard/README.md) of `scoped_lock`, a thread with that
  guard or with a lock that has `unlock()` and `lock()` over the mutex, `std::unique_lock<sgcl::async::mutex>`.
- There is no spurious wakeup: a notify wakes the waiter it took from the queue, and a waiter never leaves the queue
  on its own. The condition may still change between the notify and the mutex taken back, so the condition is
  checked under the mutex before every wait, in a loop, as everywhere.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](condition_variable.md) | constructs a condition variable with no waiter |
| `(destructor)` | destroys the condition variable, which must have no waiter |

#### Notification

| Function | Description |
|---|---|
| [notify_one](notify_one.md) | wakes the first waiter |
| [notify_all](notify_all.md) | wakes every waiter |

#### Waiting

| Function | Description |
|---|---|
| [wait](wait.md) | lets go of the mutex, waits for a notify and takes the mutex back |

## Complexity

A wait makes a channel of one signal for its waiter and pushes it on the queue, lets go of the mutex and takes it
back; a notify pops a waiter and sends it the signal.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

struct Queue {
    async::mutex lock;
    async::condition_variable changed;
    vector<int> items;  // guarded by lock
    bool closed = false;  // guarded by lock
};

async::task<int> consumer(tracked_ptr<Queue> q) {
    int sum = 0;
    auto guard = co_await q->lock.scoped_lock();
    for (;;) {
        co_await q->changed.wait(guard, [&] { return !q->items.empty() || q->closed; });
        if (q->items.empty()) {
            co_return sum;  // closed and drained
        }
        sum += q->items.back();
        q->items.pop_back();
    }
}

int main() {
    tracked_ptr q = make_tracked<Queue>();
    async::task<int> c = async::spawn(consumer(q));
    for (int i : range(1, 11)) {
        std::lock_guard guard(q->lock);  // a thread: the standard's guard
        q->items.push_back(i);
        q->changed.notify_one();
    }
    {
        std::lock_guard guard(q->lock);
        q->closed = true;
        q->changed.notify_all();
    }
    println("{}", c.wait());
}
```

Output:

```text
55
```

## See also

- [mutex](../mutex/README.md): what it waits with; [mutex::guard](../mutex-guard/README.md): the task's lock
- [event](../event/README.md): a condition set once, without a mutex
- [shared_mutex](../shared_mutex/README.md): readers and a writer
- [channel](../channel/README.md): a waiter's signal
