[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::mutex

```cpp
#include "sgcl/async/mutex.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class mutex {
    public:
        class guard;
        class scoped_lock_op;

        friend bool operator==(const mutex& a, const mutex& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::async::mutex` lets one holder at a time through, for tasks and threads alike: [lock](lock.md) is a
receive on a channel holding one signal and [unlock](unlock.md) a send, Go's idiom for a mutex. It is the first
of the synchronization of tasks, and of threads with them: the mutex, the [semaphore](../semaphore/README.md), the
[event](../event/README.md), the [wait_group](../wait_group/README.md) and the [once](../once/README.md) are each a channel of signals under the
name of what it does, with the forms of a wait a [channel](../channel/README.md) has: blocking, for a thread; awaitable, for a
task, which holds no thread while it waits; and, but for the once, a case of a [select](../select.md). After them come
a [shared_mutex](../shared_mutex/README.md), readers and a writer over a word with channels for its waits, and a
[condition_variable](../condition_variable/README.md) over this mutex.

A `std::mutex` taken on a worker parks the worker, and with it every task that worker would run; this mutex parks
nothing: a task that waits for it is a frame on the managed heap and a word on a list, as a task waiting on a
channel is, and a worker runs it when its turn comes. The price is the channel's ring and its lists of waiters, a few
hundred bytes; what it buys is one implementation, lock-free, that threads and tasks share, and a waiter the
collector reclaims. Where `std::mutex` has an owner, this one has none: whoever holds the signal holds the lock, as
with Go's `sync.Mutex`.

## Rules

- A mutex is a handle: one word, a tracked word to the state, which copies share; [operator==](operator_cmp.md)
  says whether two are the same mutex. It is made unlocked by its constructor, and there is no empty mutex
  ([README: Handles](../README.md#handles)).
- Not recursive, and no owner: a second `lock` by the holder waits for ever, and an `unlock` from another thread or
  task is a valid hand-over.
- An `unlock` without a matching lock is lost: the channel holds one signal at most, so the mutex never lets two
  holders in.
- A thread locks with [lock](lock.md), or with `std::lock_guard<sgcl::async::mutex>` and `std::unique_lock`
  over it (the mutex is the standard's Lockable); a task locks with `co_await m.scoped_lock()` and holds no thread
  while it waits ([README: Waiting operations](../README.md#waiting-operations)).
- A [guard](../mutex-guard/README.md) is an object of its scope: on a stack or in a task's frame, never in a managed object or
  a container.

## Member types

| Type | Definition |
|---|---|
| `guard` | the lock held for a scope ([guard](../mutex-guard/README.md)) |
| `scoped_lock_op` | the awaiter that `co_await` of [scoped_lock](scoped_lock.md) makes in a task |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](mutex.md) | constructs an unlocked mutex, or a handle of the same mutex |
| `(destructor)` | lets go of the handle; the state is the collector's once no handle holds it |
| [operator=](operator_assign.md) | makes the handle one of another mutex |

#### Locking

| Function | Description |
|---|---|
| [lock](lock.md) | locks the mutex, blocking the thread until it is free |
| [try_lock](try_lock.md) | locks the mutex when it is free, without waiting |
| [unlock](unlock.md) | unlocks the mutex |
| [scoped_lock](scoped_lock.md) | locks the mutex for a scope, waiting in a task or on a thread |
| [on_lock](on_lock.md) | a case of a select: a call with the mutex locked |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two handles are the same mutex |

## Complexity

Every operation is constant when the mutex is free: a lock is a receive through the channel's ring, an unlock a
send. A lock that waits registers a waiter on the channel's list, a managed node; an unlock that finds a waiter
wakes it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <mutex>

using namespace sgcl;

struct Counter {
    async::mutex lock;
    int value = 0;  // guarded by lock
};

async::task<> add(tracked_ptr<Counter> c, int n) {
    for (int i : range(1000)) {
        auto guard = co_await c->lock.scoped_lock();  // no thread held while it waits
        c->value += n;
    }
}

int main() {
    tracked_ptr c = make_tracked<Counter>();
    vector<async::task<>> tasks;
    for (int n : range(1, 5)) {
        tasks.push_back(async::spawn(add(c, n)));
    }
    for (auto& t : tasks) {
        t.wait();
    }
    std::lock_guard guard(c->lock);  // a thread: the standard's guard
    println("{}", c->value);
}
```

Output:

```text
10000
```

## See also

- [mutex::guard](../mutex-guard/README.md): the lock held for a scope
- [shared_mutex](../shared_mutex/README.md): readers and a writer
- [condition_variable](../condition_variable/README.md): a wait under this mutex
- [semaphore](../semaphore/README.md), [event](../event/README.md), [wait_group](../wait_group/README.md), [once](../once/README.md): the rest of the family
- [channel](../channel/README.md): what it is made of; [select](../select.md): its case
- [strand](../strand/README.md): serial access to a resource without a lock
- [README: Handles](../README.md#handles)
