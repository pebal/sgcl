[sgcl](../../README.md) › [async](../README.md) › [event](README.md)

# sgcl::async::event::wait, operator co_await

```cpp
void wait() const noexcept;                    // (1)
wait_op operator co_await() const noexcept;    // (2)
```

Waits for the event to be set; on an event set already, returns at once. An event is waited for as a task is
([README: Waiting operations](../README.md#waiting-operations)):

1. `e.wait()` on a thread: blocks the thread until the [set](set.md). Not from a task on a worker, which it would
   block with every task the worker runs; debug builds assert.
2. `co_await e` in a task: the task suspends, holding no thread, and is resumed by the set. The awaiter is a
   `event::wait_op` in the task's frame.

Either way the wait is a receive on the event's channel, which the set closes.

## Parameters

None.

## Return value

1. None.
2. The awaiter of `co_await e`; the `co_await` gives nothing.

## Complexity

Constant on an event set already: a look at the channel. Otherwise a waiter is registered on the channel's list
until the set wakes it.

## Exceptions

- (1) None. A receive of a channel may wake a waiting sender, whose wake may start the scheduler's workers, but
  nobody sends on an event's channel: [set](set.md) closes it.
- (2) None from the call; the `co_await` is the same receive, and is not declared `noexcept`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> worker(async::event go) {  // by value: a copy is the same event
    co_await go;
    println("started");
}

int main() {
    async::event go;
    async::task<> w = async::spawn(worker(go));
    println("set: {}", go.is_set());
    go.set();
    w.wait();
    go.wait();  // set already: returns at once
    println("done");
}
```

Output:

```text
set: false
started
done
```

## See also

- [set](set.md): what the wait waits for
- [on_set](on_set.md): the wait as a case of a select
- [is_set](is_set.md): a look without a wait
- [sgcl::async::event](README.md)
