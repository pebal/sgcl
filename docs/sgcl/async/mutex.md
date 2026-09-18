# sgcl::mutex

```cpp
#include "sgcl/async/mutex.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class mutex;   // one holder at a time
}
```

The same class in the `Sgcl` interface: [Mutex](../Sgcl/Async/Mutex.md).

One holder at a time: `lock()` is a receive on a channel holding one signal and `unlock()` a send, Go's idiom for a mutex. It is the first of the synchronization of tasks, and of threads with them: each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md). A `std::mutex` taken on a worker parks the worker, and with it every task that worker would run; these park nothing: a task that waits for one is a frame on the managed heap and a word on a list, as a task waiting on a channel is, and a worker runs it when its turn comes. What that costs: the channel's ring and its lists of waiters, a few hundred bytes each; what it buys: one implementation, lock-free, that threads and tasks share, and a waiter reclaimed by the collector. After the five, a [shared_mutex](shared_mutex.md) (readers and a writer over a word, the channels for its waits) and a [condition_variable](condition_variable.md) over this mutex, each blocking and awaitable.

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- Not recursive and no owner: whoever holds the signal holds the lock, and `unlock()` from another thread or task is a valid hand-over. `std::lock_guard<sgcl::mutex>` works for a thread; `auto guard = co_await m.async_scoped_lock();` for a task.
- An `unlock()` without a matching `lock()` is lost: the channel is full.

## Members

```cpp
void lock();  bool try_lock();  void unlock();
auto async_lock() noexcept;                    // co_await: locked
auto async_scoped_lock() noexcept;             // co_await: a guard that unlocks when destroyed
template<class F> auto on_lock(F f);           // a case of a select: f() with the lock held
class guard;                                   // the lock held for a scope; owner() is the mutex
```

```cpp
sgcl::mutex m;
int counter = 0;                                // guarded by m
auto add = [](sgcl::mutex& m, int& counter) -> sgcl::task<> {
    auto guard = co_await m.async_scoped_lock();
    ++counter;
};
std::lock_guard lock(m);                        // a thread
```

## Example

The crawler on [wait_group](wait_group.md#example): a shared count under a mutex, three fetches at once under a semaphore, an event that starts them together.

## See also

- [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md): the rest of the family; [shared_mutex](shared_mutex.md): readers and a writer; [condition_variable](condition_variable.md): a wait under this mutex
- [channel](channel.md): what it is made of; [select](select.md): the cases; [coroutine](coroutine.md), [scheduler](scheduler.md): the tasks and their workers; [executor](executor.md): a strand, serial access without a lock
- `tests/async/sync.cpp`: every behaviour above, checked.
