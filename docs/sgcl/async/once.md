# sgcl::once

```cpp
#include "sgcl/async/once.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class once;   // the first caller runs it, the others wait for it
}
```

A once: the first caller runs the function, the others wait for it to finish; `co_await o.async_call(t)` runs the task `t` once. A channel closed when the call is done, one of the family the [mutex](mutex.md)'s page describes (each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md)); the waits are blocking and awaitable.

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- A body of `call` that throws: the exception propagates and the once stays claimed, the others wait forever; a `once` is for an initialization that does not throw.

## Members

```cpp
template<class F> void call(F f);              // f() by the first caller; the others wait
template<class T> task<> async_call(task<T> t);   // co_await: the task by the first, the others wait
bool called() const noexcept;
```

```cpp
once init;
init.call([] { /* once, before anyone goes on */ });
```

## See also

- [mutex](mutex.md): the family and its three forms of a wait; [event](event.md): what the others wait on; [channel](channel.md): what it is made of
- `tests/async/sync.cpp`: every behaviour above, checked.
