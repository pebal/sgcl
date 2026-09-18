# Semaphore

```cpp
#include "sgcl/Sgcl/Async/Semaphore.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Semaphore;   // n permits
}
```

The same class in the `sgcl` interface: [semaphore](../../async/semaphore.md).

A semaphore of *n* permits: `Acquire()` takes one, `Release()` gives one back; a channel holding *n* signals, one of the family the [Mutex](Mutex.md)'s page describes (each of the five ([Mutex](Mutex.md), [Semaphore](Semaphore.md), [Event](Event.md), [WaitGroup](WaitGroup.md), [Once](Once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [Channel](Channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [Select](Select.md)).

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- A `Release()` without a matching `Acquire()` is lost (the channel is full): a semaphore never has more permits than its maximum.
- A `Semaphore(0)`, made closed, has one permit at most: a `Release()` opens it (a channel of capacity zero would lose the release with nobody waiting).

## Members

```cpp
explicit Semaphore(size_t permits, size_t max = 0);   // max: permits by default
void Acquire();  bool TryAcquire();  void Release();
auto AsyncAcquire() noexcept;                  // co_await: a permit taken
template<class F> auto OnAcquire(F f);         // a case of a Select
size_t Available() const noexcept;             // the permits free now
```

```cpp
Semaphore slots(4);                             // four at a time
auto fetch = [](Semaphore& slots) -> Task<> {
    co_await slots.AsyncAcquire();
    // ... at most four here
    slots.Release();
};
```

## Example

The crawler on [WaitGroup](WaitGroup.md#example): at most three fetches at once.

## See also

- [Mutex](Mutex.md): the family and its three forms of a wait; [Channel](Channel.md): what it is made of; [Select](Select.md): the cases
- `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
