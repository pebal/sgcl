# Once

```cpp
#include "sgcl/Sgcl/Async/Once.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Once;   // the first caller runs it, the others wait for it
}
```

The same class in the `sgcl` interface: [once](../../async/once.md).

A once: the first caller runs the function, the others wait for it to finish; `co_await o.AsyncCall(t)` runs the task `t` once. A channel closed when the call is done, one of the family the [Mutex](Mutex.md)'s page describes (each of the five ([Mutex](Mutex.md), [Semaphore](Semaphore.md), [Event](Event.md), [WaitGroup](WaitGroup.md), [Once](Once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [Channel](Channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [Select](Select.md)); the waits are blocking and awaitable.

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- A body of `Call` that throws: the exception propagates and the once stays claimed, the others wait forever; a `Once` is for an initialization that does not throw.

## Members

```cpp
template<class F> void Call(F f);              // f() by the first caller; the others wait
template<class T> Task<> AsyncCall(Task<T> t);   // co_await: the task by the first, the others wait
bool IsCalled() const noexcept;
```

```cpp
Once init;
init.Call([] { /* once, before anyone goes on */ });
```

## See also

- [Mutex](Mutex.md): the family and its three forms of a wait; [Event](Event.md): what the others wait on; [Channel](Channel.md): what it is made of
- `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
