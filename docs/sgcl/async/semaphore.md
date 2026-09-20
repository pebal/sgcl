# sgcl::semaphore

```cpp
#include "sgcl/async/semaphore.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class semaphore;   // n permits
}
```

A semaphore of *n* permits: `acquire()` takes one, `release()` gives one back; a channel holding *n* signals, one of the family the [mutex](mutex.md)'s page describes (each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md)).

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- A `release()` without a matching `acquire()` is lost (the channel is full): a semaphore never has more permits than its maximum.
- A `semaphore(0)`, made closed, has one permit at most: a `release()` opens it (a channel of capacity zero would lose the release with nobody waiting).

## Members

```cpp
explicit semaphore(size_t permits, size_t max = 0);   // max: permits by default
void acquire();  bool try_acquire();  void release();
auto async_acquire() noexcept;                 // co_await: a permit taken
template<class F> auto on_acquire(F f);        // a case of a select
size_t available() const noexcept;             // the permits free now
```

```cpp
sgcl::semaphore slots(4);                       // four at a time
auto fetch = [](sgcl::semaphore& slots) -> sgcl::task<> {
    co_await slots.async_acquire();
    // ... at most four here
    slots.release();
};
```

## Example

The crawler on [wait_group](wait_group.md#example): at most three fetches at once.

## See also

- [mutex](mutex.md): the family and its three forms of a wait; [channel](channel.md): what it is made of; [select](select.md): the cases
- `tests/async/sync.cpp`: every behaviour above, checked.
