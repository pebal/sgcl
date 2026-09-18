# WaitGroup

```cpp
#include "sgcl/Sgcl/Async/WaitGroup.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class WaitGroup;   // counts work down to zero
}
```

The same class in the `sgcl` interface: [wait_group](../../async/wait_group.md).

A wait group: `Add(n)` counts the work, `Done()` counts it off, `Wait()` waits for the count to reach zero; a group whose count came back from zero (an `Add` after the work was done) is waited for again, as Go's is. Under it a channel per round, closed when the count reaches zero and replaced by the `Add` that starts the next round; the old ones are the collector's. One of the family the [Mutex](Mutex.md)'s page describes (each of the five ([Mutex](Mutex.md), [Semaphore](Semaphore.md), [Event](Event.md), [WaitGroup](WaitGroup.md), [Once](Once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [Channel](Channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [Select](Select.md)). A [TaskGroup](TaskGroup.md) is a wait group over the children it spawns, with their exceptions and their cancellation.

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- `Add` before the work starts and `Wait` after are the program's order, as Go's are: an `Add` racing with a `Wait` from zero is a misuse.
- `Add(-n)` takes n off, as `Done()` takes one: a count brought to zero releases the waiters (Go's `Add(-n)`).

## Members

```cpp
void Add(long n = 1);  void Done();  long Count() const noexcept;
void Wait();                                   // a thread
Task<> AsyncWait();                            // co_await: the task resumed at zero
template<class F> auto OnDone(F f);            // a case of a Select: f() when the count is zero
```

```cpp
WaitGroup all;
all.Add(2);
Go([](WaitGroup& all) -> Task<> { all.Done(); co_return; }(all));
Go([](WaitGroup& all) -> Task<> { all.Done(); co_return; }(all));
all.Wait();                                     // both done
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A crawler with a bound: at most three fetches at once (the semaphore),
// a shared count under a mutex, a wait group that knows when all is
// done, and an event that starts them together. Every wait here is a
// task's, no thread held.
struct Site {
    Semaphore slots{3};
    Mutex lock;
    WaitGroup pending;
    Event go;
    int fetched = 0;   // guarded by lock
};

Task<> Fetch(Ptr<Site> site, int page) {
    co_await site->go.AsyncWait();                           // all start together
    co_await site->slots.AsyncAcquire();                     // three at a time
    co_await Sleep(1ms);                                     // the fetch
    {
        auto guard = co_await site->lock.AsyncScopedLock();
        ++site->fetched;
    }
    site->slots.Release();
    site->pending.Done();
}

int main() {
    Ptr site = Make<Site>();
    for (int page : Range(20)) {
        site->pending.Add();
        Go(Fetch(site, page));
    }
    site->go.Set();
    site->pending.Wait();                                    // this thread waits for the twenty
    std::cout << site->fetched << " pages\n";                // 20 pages
    return site->fetched == 20 ? 0 : 1;
}
```

The output:

```
20 pages
```

## See also

- [Mutex](Mutex.md): the family and its three forms of a wait; [TaskGroup](TaskGroup.md): the wait group with the children's exceptions and cancellation; [WhenAll, WhenAny](When.md): every result back; [Channel](Channel.md): what it is made of; [Select](Select.md): the cases
- `tests/Sgcl/sgcl.cpp`, `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
