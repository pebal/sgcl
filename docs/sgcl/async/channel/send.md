[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::send

```cpp
auto send(const T& value) const                                                  // (1)
    noexcept(std::is_nothrow_copy_constructible_v<T> &&
             std::is_nothrow_move_constructible_v<T>);
auto send(T&& value) const noexcept(std::is_nothrow_move_constructible_v<T>);    // (2)
auto send() const noexcept;                                                      // (3)
```

Sends an element, waiting for room in the buffer or, on a rendezvous, for a receiver. The call makes an
[operation](../operation/README.md) that holds the element and does nothing yet; carried out, the send hands the element to
a waiting receiver when the buffer is empty, or puts it in the buffer when there is room, or waits with it until a
receiver moves it into the buffer or, on a rendezvous, takes it.

1. Sends a copy of `value`.
2. Sends `value`, moved.
3. `channel<void>`: sends a signal.

A task writes `co_await ch.send(v)`: it is suspended while the send waits, with no thread held, and resumed on a
worker by the receive that takes the element. A thread writes `ch.send(v).wait()` and blocks
([README: Waiting operations](../README.md#waiting-operations)). Waiting senders are served in their order: a
waiting sender's element is moved into the buffer, or a rendezvous's small ring, behind the ones before it.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to send |

## Return value

An [operation](../operation/README.md) that gives a `bool`, both by `co_await` in a task and by `.wait()` on a thread:
`true` when the element was delivered, to a receiver or into the buffer; `false` when the channel was closed before
the send or while it waited, the element undelivered and dropped with the operation.

## Complexity

Constant, one compare-exchange on the ring, when the send does not wait; plus the wake of the receiver it serves.
A send that waits allocates its waiter, one managed object.

## Exceptions

- The call: (1) what the copy or the move constructor of `T` throws, (2) what the move constructor of `T` throws;
  none when they are noexcept. (3) None.
- Carried out: `std::system_error` when the send wakes a waiting task and the wake starts the scheduler's workers,
  one of which cannot be started; what the move constructor of `T` throws.

A move of `T` that throws leaves the channel as it was: the element is not delivered, a receiver it was
being handed to waits on, and a slot of the buffer it was being moved into is passed over by the receives. A send
that waits throws what the move of its element into the buffer threw, made by the receive that served it.

## Notes

A send to a closed channel gives `false` where Go panics. A debug build asserts on an operation made and never
carried out: `ch.send(v);` alone sends nothing. `.wait()` is never written in a task, where it would hold a worker
from every other task.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> send_three(async::channel<string> words) {
    for (const char* w : {"one", "two", "three"}) {
        co_await words.send(w);  // suspended until the receive takes it
    }
    words.close();
}

int main() {
    async::channel<string> words;  // a rendezvous
    async::go(send_three(words));
    while (auto w = words.receive().wait()) {
        println("{}", *w);
    }
    println("{}", words.send("four").wait());  // closed: not delivered

    async::channel<void> ready(1);
    println("{}", ready.send().wait());
}
```

Output:

```text
one
two
three
false
true
```

## See also

- [try_send](try_send.md): sends only when it can without waiting
- [on_send](on_send.md): a send as a case of a select
- [receive](receive.md), [close](close.md)
- [sgcl::async::channel\<T\>](README.md)
