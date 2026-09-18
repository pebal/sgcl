# Mutex

```cpp
#include "sgcl/Sgcl/Async/Mutex.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Mutex;   // one holder at a time
}
```

The same class in the `sgcl` interface: [mutex](../../async/mutex.md).

One holder at a time: `Lock()` is a receive on a channel holding one signal and `Unlock()` a send, Go's idiom for a mutex. It is the first of the synchronization of tasks, and of threads with them: each of the five ([Mutex](Mutex.md), [Semaphore](Semaphore.md), [Event](Event.md), [WaitGroup](WaitGroup.md), [Once](Once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [Channel](Channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [Select](Select.md). A `std::mutex` taken on a worker parks the worker, and with it every task that worker would run; these park nothing: a task that waits for one is a frame on the managed heap and a word on a list, as a task waiting on a channel is, and a worker runs it when its turn comes. What that costs: the channel's ring and its lists of waiters, a few hundred bytes each; what it buys: one implementation, lock-free, that threads and tasks share, and a waiter reclaimed by the collector. After the five, a [SharedMutex](SharedMutex.md) (readers and a writer over a word, the channels for its waits) and a [ConditionVariable](ConditionVariable.md) over this mutex, each blocking and awaitable.

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- Not recursive and no owner: whoever holds the signal holds the lock, and `Unlock()` from another thread or task is a valid hand-over. `std::lock_guard<Mutex>` works for a thread (the class has `lock` and `unlock` for it); `auto guard = co_await m.AsyncScopedLock();` for a task.
- An `Unlock()` without a matching `Lock()` is lost: the channel is full.

## Members

```cpp
void Lock();  bool TryLock();  void Unlock();
auto AsyncLock() noexcept;                     // co_await: locked
auto AsyncScopedLock() noexcept;               // co_await: a Guard that unlocks when destroyed
template<class F> auto OnLock(F f);            // a case of a Select: f() with the lock held
using Guard = sgcl::mutex::guard;              // the lock held for a scope
void lock();  void unlock();                   // for std::lock_guard<Mutex>
```

```cpp
Mutex m;
int counter = 0;                                // guarded by m
auto add = [](Mutex& m, int& counter) -> Task<> {
    auto guard = co_await m.AsyncScopedLock();
    ++counter;
};
std::lock_guard lock(m);                        // a thread
```

## Example

The crawler on [WaitGroup](WaitGroup.md#example): a shared count under a mutex, three fetches at once under a semaphore, an event that starts them together.

## See also

- [Semaphore](Semaphore.md), [Event](Event.md), [WaitGroup](WaitGroup.md), [Once](Once.md): the rest of the family; [SharedMutex](SharedMutex.md): readers and a writer; [ConditionVariable](ConditionVariable.md): a wait under this mutex
- [Channel](Channel.md): what it is made of; [Select](Select.md): the cases; [Task](Task.md), [Scheduler](Scheduler.md): the tasks and their workers; [Executor](Executor.md): a Strand, serial access without a lock
- `tests/Sgcl/sgcl.cpp`, `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
