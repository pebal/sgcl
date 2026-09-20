# sgcl::wait_group

```cpp
#include "sgcl/async/wait_group.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class wait_group;   // counts work down to zero
}
```

A wait group: `add(n)` counts the work, `done()` counts it off, `wait()` waits for the count to reach zero; a group whose count came back from zero (an `add` after the work was done) is waited for again, as Go's is. Under it a channel per round, closed when the count reaches zero and replaced by the `add` that starts the next round; the old ones are the collector's. One of the family the [mutex](mutex.md)'s page describes (each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md)). A [task_group](task_group.md) is a wait group over the children it spawns, with their exceptions and their cancellation.

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- `add` before the work starts and `wait` after are the program's order, as Go's are: an `add` racing with a `wait` from zero is a misuse.
- `add(-n)` takes n off, as `done()` takes one: a count brought to zero releases the waiters (Go's `Add(-n)`).

## Members

```cpp
void add(long n = 1);  void done();  long count() const noexcept;
void wait();                                   // a thread
task<> async_wait();                           // co_await: the task resumed at zero
template<class F> auto on_done(F f);           // a case of a select: f() when the count is zero
```

```cpp
sgcl::wait_group all;
all.add(2);
sgcl::go([](sgcl::wait_group& all) -> sgcl::task<> { all.done(); co_return; }(all));
sgcl::go([](sgcl::wait_group& all) -> sgcl::task<> { all.done(); co_return; }(all));
all.wait();                                     // both done
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace std::chrono_literals;

// A crawler with a bound: at most three fetches at once (the semaphore),
// a shared count under a mutex, a wait group that knows when all is
// done, and an event that starts them together. Every wait here is a
// task's, no thread held.
struct Site {
    sgcl::semaphore slots{3};
    sgcl::mutex lock;
    sgcl::wait_group pending;
    sgcl::event go;
    int fetched = 0;   // guarded by lock
};

sgcl::task<> fetch(sgcl::tracked_ptr<Site> site, int page) {
    co_await site->go.async_wait();                          // all start together
    co_await site->slots.async_acquire();                    // three at a time
    co_await sgcl::sleep(1ms);                               // the fetch
    {
        auto guard = co_await site->lock.async_scoped_lock();
        ++site->fetched;
    }
    site->slots.release();
    site->pending.done();
}

int main() {
    sgcl::tracked_ptr site = sgcl::make_tracked<Site>();
    for (int page : sgcl::range(20)) {
        site->pending.add();
        sgcl::go(fetch(site, page));
    }
    site->go.set();
    site->pending.wait();                                    // this thread waits for the twenty
    std::cout << site->fetched << " pages\n";                // 20 pages
    return site->fetched == 20 ? 0 : 1;
}
```

The output:

```
20 pages
```

## See also

- [mutex](mutex.md): the family and its three forms of a wait; [task_group](task_group.md): the wait group with the children's exceptions and cancellation; [when](when.md): every result back; [channel](channel.md): what it is made of; [select](select.md): the cases
- `tests/async/sync.cpp`: every behaviour above, checked.
