# sgcl::event

```cpp
#include "sgcl/async/event.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class event;   // set once, waited for by any number
}
```

An event: set once, waited for by any number, and a wait after the set does not wait; a channel closed by `set()`, one of the family the [mutex](mutex.md)'s page describes (each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md)). A [promise](promise.md) is an event with a value.

## Rules

- It lives where a `tracked_ptr` may: on a stack or inside a managed object ([The rules](../core/README.md#the-rules), 1); not copyable, not movable.
- Set once: there is no reset. A wait after the set returns at once.

## Members

```cpp
void set();  bool is_set() const noexcept;
void wait();                                   // a thread
auto async_wait() noexcept;                    // co_await: the task resumed by the set
template<class F> auto on_set(F f);            // a case of a select
```

```cpp
event ready;
auto worker = [](event& ready) -> task<> {
    co_await ready.async_wait();                // all start together
};
ready.set();
```

## Example

The crawler on [wait_group](wait_group.md#example): an event that starts the fetches together.

## See also

- [mutex](mutex.md): the family and its three forms of a wait; [promise](promise.md): an event with a value; [channel](channel.md): what it is made of; [select](select.md): the cases
- `tests/async/sync.cpp`: every behaviour above, checked.
