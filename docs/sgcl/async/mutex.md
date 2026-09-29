# sgcl::async::mutex

```cpp
#include "sgcl/async/mutex.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class mutex;   // one holder at a time
}
```

One holder at a time: `lock()` is a receive on a channel holding one signal and `unlock()` a send, Go's idiom for a mutex. It is the first of the synchronization of tasks, and of threads with them: each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md). A `std::mutex` taken on a worker parks the worker, and with it every task that worker would run; these park nothing: a task that waits for one is a frame on the managed heap and a word on a list, as a task waiting on a channel is, and a worker runs it when its turn comes. What that costs: the channel's ring and its lists of waiters, a few hundred bytes each; what it buys: one implementation, lock-free, that threads and tasks share, and a waiter reclaimed by the collector. After the five, a [shared_mutex](shared_mutex.md) (readers and a writer over a word, the channels for its waits) and a [condition_variable](condition_variable.md) over this mutex, each blocking and awaitable.

## Rules

- A handle: one word, a tracked word to the state, which copies share (`==` says whether two are the same). Made by the constructor; there is no empty mutex. It lies on a stack, in a task (a parameter by value), in a managed object; in a global or a std container, as a `rooted<async::mutex>` ([rooted](../core/rooted.md)), the same object reached with `->`. A root is never part of a cycle: never a `rooted` in a managed object or a task's frame ([The rules](../core/README.md#the-rules), 1).
- Not recursive and no owner: whoever holds the signal holds the lock, and `unlock()` from another thread or task is a valid hand-over. `std::lock_guard<sgcl::async::mutex>` works for a thread; `auto guard = co_await m.scoped_lock();` for a task.
- An `unlock()` without a matching `lock()` is lost: the channel is full.
- A `guard` is an object of its scope, one word, movable and not copyable: only on a stack or in a task's frame (both scanned conservatively); never in a managed object or a container, as any raw pointer ([The rules](../core/README.md#the-rules), 3).

## Members

```cpp
mutex();                                             // unlocked
void lock() const;  bool try_lock() const;  void unlock() const;
auto scoped_lock() const;                            // co_await: a guard that unlocks when destroyed; .wait() on a thread
template<class F> auto on_lock(F f) const;           // a case of a select: f() with the lock held
class guard;                                         // the lock held for a scope, one word; owner() is the mutex, release() gives it up locked
friend bool operator==(const mutex&, const mutex&) noexcept;   // the same mutex
```

```cpp
async::mutex m;
int counter = 0;                                // guarded by m
auto add = [](async::mutex m, int& counter) -> async::task<> {   // the mutex by value: a copy is the same mutex
    auto guard = co_await m.scoped_lock();
    ++counter;
};
std::lock_guard lock(m);                        // a thread
```

## Example

Two tasks add to a count under the mutex, one holder at a time; the thread then takes it with the standard's guard:

```cpp
#include "sgcl/async/async.h"
#include "sgcl/io/io.h"
#include <mutex>

using namespace sgcl;

struct Counter {
    async::mutex lock;
    int value = 0;   // guarded by lock
};

async::task<> add(tracked_ptr<Counter> c, int n) {
    auto guard = co_await c->lock.scoped_lock();   // no thread held while it waits
    c->value += n;
}

int main() {
    tracked_ptr c = make_tracked<Counter>();
    auto a = async::spawn(add(c, 1));
    auto b = async::spawn(add(c, 2));
    a.wait();
    b.wait();
    std::lock_guard guard(c->lock);
    println("{}", c->value);
}
```

Output:

```text
3
```

## See also

- [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md): the rest of the family; [shared_mutex](shared_mutex.md): readers and a writer; [condition_variable](condition_variable.md): a wait under this mutex
- [channel](channel.md): what it is made of; [select](select.md): the cases; [coroutine](coroutine.md), [scheduler](scheduler.md): the tasks and their workers; [executor](executor.md): a strand, serial access without a lock
- `tests/async/sync.cpp`: every behaviour above, checked.
