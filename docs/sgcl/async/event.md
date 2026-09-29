# sgcl::async::event

```cpp
#include "sgcl/async/event.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class event;   // set once, waited for by any number
}
```

An event: set once, waited for by any number, and a wait after the set does not wait; a channel closed by `set()`, one of the family the [mutex](mutex.md)'s page describes (each of the five ([mutex](mutex.md), [semaphore](semaphore.md), [event](event.md), [wait_group](wait_group.md), [once](once.md)) a channel of signals under the name of what it does, with the three forms of a wait a [channel](channel.md) has: blocking, for a thread; awaitable, for a task, which holds no thread while it waits; and as a case of a [select](select.md)). A [promise](promise.md) is an event with a value.

## Rules

- A handle: one word, a tracked word to the state, which copies share (`==` says whether two are the same). Made by the constructor; there is no empty event. It lies on a stack, in a task (a parameter by value), in a managed object; in a global or a std container, as a `rooted<async::event>` ([rooted](../core/rooted.md)), the same object reached with `->`. A root is never part of a cycle: never a `rooted` in a managed object or a task's frame ([The rules](../core/README.md#the-rules), 1).
- Set once: there is no reset. A wait after the set returns at once.
- An event happens once, and a wait ends when it has happened: after `wait()`, `co_await` or a select's `on_set` case, `is_set()` is true — for the events of the reactor and the timers too.
- What the [reactor](reactor.md) and the [timers](timer.md) give (`readable`, `writable`, `exited`, `after`, `at`) is an event: `co_await async::readable(fd)` in a task, `async::after(1s).wait()` on a thread, `.on_set(f)` in a select. It is set when the moment comes or when the wait is ended with nothing (a cancel, a stop): a wait woken by it looks at its source again.

## Members

```cpp
event();                                             // not set
void set() const;  bool is_set() const noexcept;
void wait() const;                                   // a thread
wait_op operator co_await() const;                   // co_await e: the task resumed by the set
template<class F> auto on_set(F f) const;            // a case of a select
friend bool operator==(const event&, const event&) noexcept;   // the same event
```

```cpp
async::event ready;
auto worker = [](async::event ready) -> async::task<> {   // by value: a copy is the same event
    co_await ready;                // all start together
};
ready.set();
```

## Example

A task waits for the event with no thread held; a wait after the set returns at once:

```cpp
#include "sgcl/async/async.h"
#include "sgcl/io/io.h"

using namespace sgcl;

async::task<> worker(async::event go) {   // by value: a copy is the same event
    co_await go;
    println("started");
}

int main() {
    async::event go;
    auto w = async::spawn(worker(go));
    println("set: {}", go.is_set());
    go.set();
    w.wait();
    go.wait();   // set already
}
```

Output:

```text
set: false
started
```

## See also

- [mutex](mutex.md): the family and its three forms of a wait; [promise](promise.md): an event with a value; [channel](channel.md): what it is made of; [select](select.md): the cases
- `tests/async/sync.cpp`: every behaviour above, checked.
