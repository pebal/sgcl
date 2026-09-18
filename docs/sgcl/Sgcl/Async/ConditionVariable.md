# ConditionVariable

```cpp
#include "sgcl/Sgcl/Async/ConditionVariable.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class ConditionVariable;   // a wait under the Mutex for a notify
}
```

The same class in the `sgcl` interface: [condition_variable](../../async/condition_variable.md).

Go's `sync.Cond` and `std::condition_variable_any` over the module's [Mutex](Mutex.md), for tasks and threads alike: a wait lets go of the mutex, waits for a notify and takes the mutex back; a task waiting holds no thread, and takes the mutex back with a `co_await` too. Under it a queue of waiters, each a channel of one signal: `NotifyOne` takes the first and sends it the signal, `NotifyAll` every one. A waiter is on the queue before the mutex is let go of, so a notify made under the mutex after the wait began reaches it, and a signal sent before the waiter reaches its receive is kept for it: no wakeup is lost between the unlock and the wait. The one that is lost is the one a condition variable loses by contract: a notify before the wait began finds no waiter, so the waiter checks its condition under the mutex before it waits (`Wait(guard, predicate)` does, and so must every use of the plain `Wait`), as Go's and the standard's must.

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- It waits with the module's `Mutex` only: a `Mutex::Guard` (a task's, from `AsyncScopedLock`) or any lock with `unlock()` and `lock()` over it (`std::unique_lock<Mutex>`, a thread's).
- The condition is checked under the mutex before every wait. There is no spurious wakeup (a notify wakes the waiter it took from the queue, and a waiter never leaves the queue on its own), but the condition may change between the notify and the mutex taken back, so a loop over the predicate is the form, as everywhere.

## Members

```cpp
void NotifyOne();  void NotifyAll();
void Wait(Mutex::Guard& guard);                          // a thread: the mutex let go of, the wait, the mutex taken back
template<class Lock> void Wait(Lock& lock);              // the same with a lock that has unlock() and lock(): std::unique_lock<Mutex>
template<class Lock, class Pred> void Wait(Lock& lock, Pred pred);   // until pred(), checked under the mutex before every wait
Task<> AsyncWait(Mutex::Guard& guard);                   // a task: co_await; the mutex taken back with a co_await too
template<class Pred> Task<> AsyncWait(Mutex::Guard& guard, Pred pred);
```

```cpp
Mutex m;
ConditionVariable changed;
bool flag = false;                              // guarded by m
auto waitForFlag = [](Mutex& m, ConditionVariable& changed, bool& flag) -> Task<> {
    auto guard = co_await m.AsyncScopedLock();
    co_await changed.AsyncWait(guard, [&] { return flag; });   // the mutex held again here
};
```

## Example

The table on [SharedMutex](SharedMutex.md#example): the readers' results handed to the main thread through a queue under a mutex with a condition variable, the main thread waiting with `std::unique_lock` over the module's Mutex and the predicate.

## See also

- [Mutex](Mutex.md): what it waits with; [Event](Event.md): a condition set once, without the mutex; [SharedMutex](SharedMutex.md): readers and a writer; [Channel](Channel.md): a waiter's signal
- `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
