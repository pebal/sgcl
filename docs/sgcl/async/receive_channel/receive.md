[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::receive_channel\<T\>::receive

```cpp
auto receive() const noexcept;    // (1)
auto receive() const noexcept;    // (2), receive_channel<void>
```

Receives the next element of the channel, waiting for one: the channel's [receive](../channel/receive.md), the same
operation. The call makes an [operation](../operation/README.md) that does nothing yet; a task writes
`co_await in.receive()`, suspended with no thread held until a send serves it, and a thread writes
`in.receive().wait()` and blocks. A closed channel gives what was sent before the close, then nothing at once.

1. Receives an element.
2. `receive_channel<void>`: receives a signal.

## Parameters

None.

## Return value

An [operation](../operation/README.md) that gives, both by `co_await` in a task and by `.wait()` on a thread:

- (1) an `optional<T>` with the element, or `nullopt` once the channel is closed and drained;
- (2) `true` for a signal, `false` once the channel is closed and drained.

## Complexity

Constant, one compare-exchange on the ring, when the receive does not wait; plus the wake of the sender whose
element it moved in. A receive that waits allocates its waiter, one managed object.

## Exceptions

- The call: none.
- Carried out: `std::system_error` when the receive wakes a waiting sender's task and the wake starts the
  scheduler's workers, one of which cannot be started; what the move constructor of `T` throws.

A move of `T` that throws loses the element being moved and leaves the channel otherwise as it was: its slot
of the buffer is free for the sends after it. A receive that waits throws what the move of its element into it
threw, made by the send that served it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> count(async::receive_channel<void> ticks) {
    int n = 0;
    while (co_await ticks.receive()) {
        ++n;
    }
    co_return n;
}

int main() {
    async::channel<void> ticks;
    async::task<int> counting = async::spawn(count(ticks));
    ticks.send().wait();
    ticks.send().wait();
    ticks.close();
    println("{}", counting.wait());
}
```

Output:

```text
2
```

## See also

- [try_receive](try_receive.md): receives only what is there
- [on_receive](on_receive.md): a receive as a case of a select
- [channel::receive](../channel/receive.md): the same receive
- [sgcl::async::receive_channel\<T\>](README.md)
