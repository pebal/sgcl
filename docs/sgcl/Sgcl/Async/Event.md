# Event

```cpp
#include "sgcl/Sgcl/Async/Event.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    class Event;   // set once, waited for by any number
}
```

The same class in the `sgcl` interface: [event](../../async/event.md).

An event: set once, waited for by any number, and a wait after the set does not wait; a channel closed by `Set()`, one of the family the [Mutex](Mutex.md)'s page describes (each of the five ([Mutex](Mutex.md), [Semaphore](Semaphore.md), [Event](Event.md), [WaitGroup](WaitGroup.md), [Once](Once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [Channel](Channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [Select](Select.md)). A [Promise](Promise.md) is an event with a value.

## Rules

- It lives where a `Ptr` may: on a stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1); not copyable, not movable.
- Set once: there is no reset. A wait after the set returns at once.

## Members

```cpp
void Set();  bool IsSet() const noexcept;
void Wait();                                   // a thread
auto AsyncWait() noexcept;                     // co_await: the task resumed by the set
template<class F> auto OnSet(F f);             // a case of a Select
```

```cpp
Event ready;
auto worker = [](Event& ready) -> Task<> {
    co_await ready.AsyncWait();                 // all start together
};
ready.Set();
```

## Example

The crawler on [WaitGroup](WaitGroup.md#example): an event that starts the fetches together.

## See also

- [Mutex](Mutex.md): the family and its three forms of a wait; [Promise](Promise.md): an event with a value; [Channel](Channel.md): what it is made of; [Select](Select.md): the cases
- `tests/Sgcl/sync_and_broadcast.cpp`: the behaviour above, checked.
