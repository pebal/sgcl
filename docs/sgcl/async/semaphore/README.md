[sgcl](../../README.md) › [async](../README.md)

# sgcl::async::semaphore

```cpp
#include "sgcl/async/semaphore.h"   // or "sgcl/async.h"

namespace sgcl::async {
    class semaphore;
}
```

`sgcl::async::semaphore` holds *n* permits: [acquire](acquire.md) takes one, waiting while there is none,
and [release](release.md) gives one back. Under it is a channel holding *n* signals, Go's idiom of a
buffered channel as a counting semaphore, so it is waited for the ways a [channel](../channel/README.md) is: blocking on a
thread, awaited in a task, which holds no thread while it waits, and as a case of a [select](../select.md). It is one
of the family the [mutex](../mutex/README.md) heads, each a channel of signals under the name of what it does.

What differs from `std::counting_semaphore`: the maximum is a value of the constructor, not a template argument, a
release past the maximum is lost rather than undefined, and a task that waits parks no worker.

## Rules

- A semaphore is an object, not a handle: it lives where a `tracked_ptr` may, on a stack or inside a managed object
  ([The rules](../../core/README.md#the-rules), 1), and is neither copied nor moved. Tasks reach it through the
  object that holds it.
- A semaphore never holds more permits than its maximum: a [release](release.md) that finds the channel
  full is lost.
- `semaphore(0)`, made closed, has a maximum of one: a release opens it, which a channel of capacity zero, a
  rendezvous, would lose when nobody waits.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](semaphore.md) | constructs a semaphore with its permits and its maximum |
| `(destructor)` | destroys the semaphore; its channel is the collector's |

#### Operations

| Function | Description |
|---|---|
| [acquire](acquire.md) | takes a permit, waiting in a task or on a thread for one |
| [try_acquire](try_acquire.md) | takes a permit when there is one, without waiting |
| [release](release.md) | gives a permit back |
| [on_acquire](on_acquire.md) | a case of a select: a call with a permit taken |

#### Observers

| Function | Description |
|---|---|
| [available](available.md) | the permits free now |

## Complexity

An acquire is a receive through the channel's ring and a release a send, constant when they do not wait; an
acquire that waits registers a waiter on the channel's list, and a release that finds one wakes it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Pool {
    async::semaphore slots{3};
    atomic<int> inside = 0;
    atomic<int> most = 0;
};

async::task<> fetch(tracked_ptr<Pool> pool) {
    co_await pool->slots.acquire();  // three at a time
    int now = ++pool->inside;
    int seen = pool->most.load();
    while (now > seen && !pool->most.compare_exchange_weak(seen, now)) {
    }
    co_await async::yield();  // the work
    --pool->inside;
    pool->slots.release();
}

int main() {
    tracked_ptr pool = make_tracked<Pool>();
    vector<async::task<>> tasks;
    for (int i : range(20)) {
        tasks.push_back(async::spawn(fetch(pool)));
    }
    for (auto& t : tasks) {
        t.wait();
    }
    println("never more than 3 at once: {}", pool->most.load() <= 3);
    println("{} free", pool->slots.available());
}
```

Output:

```text
never more than 3 at once: true
3 free
```

## See also

- [mutex](../mutex/README.md): one holder, and the family of the synchronization
- [channel](../channel/README.md): what it is made of; [select](../select.md): its case
- [wait_group](../wait_group/README.md): the wait for a count to reach zero
