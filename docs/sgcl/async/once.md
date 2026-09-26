# sgcl::async::once

```cpp
#include "sgcl/async/once.h"   // or "sgcl/sgcl.h"

namespace sgcl::async {
    class once;   // the first caller runs it, the others wait for it
}
```

A once: the first caller runs the function, the others wait for it to finish. `call(f)` is an [operation](README.md#waiting-operations), waited for as every wait of the module: `co_await o.call(f)` in a task (the tasks that lose wait holding no worker), `o.call(f).wait()` on a thread (not from a task on a worker; debug builds assert); `co_await o.call(t)` runs the task `t` once. A channel closed when the call is done, one of the family the [mutex](mutex.md)'s page describes (each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does); a once's waits are two of the forms a [channel](channel.md) has: blocking, for a thread, and awaitable, for a task, which holds no thread while it waits. A once is not a case of a [select](select.md).

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- A body of `call` that throws ends the once as a promise set with an exception does: the exception is kept, the once is done (`called()` is `true`), the first caller gets it thrown and so does every caller then waiting or coming later. The body is not run again.

## Members

```cpp
template<class F> auto call(F f);              // an operation: f() by the first caller, the others wait; co_await it or .wait()
template<class T> async::task<> call(async::task<T> t);   // co_await: the task by the first, the others wait
template<class F> async::task<> call(F f);     // F a coroutine function (a lambda returning a task, captures and all): its task by the first
bool called() const noexcept;
```

```cpp
async::once init;
init.call([] { /* once, before anyone goes on */ }).wait();   // a thread; a task: co_await init.call(...)
```

## See also

- [mutex](mutex.md): the family and its three forms of a wait; [event](event.md): what the others wait on; [channel](channel.md): what it is made of
- `tests/async/sync.cpp`: every behaviour above, checked.
