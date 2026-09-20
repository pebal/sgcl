# sgcl::condition_variable

```cpp
#include "sgcl/async/condition_variable.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class condition_variable;   // a wait under the mutex for a notify
}
```

Go's `sync.Cond` and `std::condition_variable_any` over the module's [mutex](mutex.md), for tasks and threads alike: a wait lets go of the mutex, waits for a notify and takes the mutex back; a task waiting holds no thread, and takes the mutex back with a `co_await` too. Under it a queue of waiters, each a channel of one signal: `notify_one` takes the first and sends it the signal, `notify_all` every one. A waiter is on the queue before the mutex is let go of, so a notify made under the mutex after the wait began reaches it, and a signal sent before the waiter reaches its receive is kept for it: no wakeup is lost between the unlock and the wait. The one that is lost is the one a condition variable loses by contract: a notify before the wait began finds no waiter, so the waiter checks its condition under the mutex before it waits (`wait(guard, predicate)` does, and so must every use of the plain `wait`), as Go's and the standard's must.

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- It waits with the module's `mutex` only: a `mutex::guard` (a task's, from `async_scoped_lock`) or any lock with `unlock()` and `lock()` over it (`std::unique_lock<sgcl::mutex>`, a thread's).
- The condition is checked under the mutex before every wait. There is no spurious wakeup (a notify wakes the waiter it took from the queue, and a waiter never leaves the queue on its own), but the condition may change between the notify and the mutex taken back, so a loop over the predicate is the form, as everywhere.

## Members

```cpp
void notify_one();  void notify_all();
void wait(mutex::guard& g);                              // a thread: the mutex let go of, the wait, the mutex taken back
template<class Lock> void wait(Lock& lock);              // the same with a lock that has unlock() and lock(): std::unique_lock<sgcl::mutex>
template<class Lock, class Pred> void wait(Lock& lock, Pred pred);   // until pred(), checked under the mutex before every wait
task<> async_wait(mutex::guard& g);                      // a task: co_await; the mutex taken back with a co_await too
template<class Pred> task<> async_wait(mutex::guard& g, Pred pred);
```

```cpp
sgcl::mutex m;
sgcl::condition_variable changed;
bool flag = false;                              // guarded by m
auto wait_for_flag = [](sgcl::mutex& m, sgcl::condition_variable& changed, bool& flag) -> sgcl::task<> {
    auto guard = co_await m.async_scoped_lock();
    co_await changed.async_wait(guard, [&] { return flag; });   // the mutex held again here
};
```

## Example

The table on [shared_mutex](shared_mutex.md#example): the readers' results handed to the main thread through a queue under a mutex with a condition variable, the main thread waiting with `std::unique_lock` over the module's mutex and the predicate.

## See also

- [mutex](mutex.md): what it waits with; [event](event.md): a condition set once, without the mutex; [shared_mutex](shared_mutex.md): readers and a writer; [channel](channel.md): a waiter's signal
- `tests/async/condition_variable.cpp`: every behaviour above, checked.
