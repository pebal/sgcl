[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::receive

```cpp
auto receive() const noexcept;    // (1)
auto receive() const noexcept;    // (2), channel<void>
```

Receives the next element, waiting for one. The call makes an [operation](../operation.md) that does nothing yet;
carried out, the receive takes the first element of the buffer, moving the first waiting sender's element in behind
it so that the order of the sends holds; with the buffer empty it takes a waiting sender's element through the
buffer, or on a rendezvous through its small ring, or waits until a send comes or the channel is closed.

1. Receives an element.
2. `channel<void>`: receives a signal.

A task writes `co_await ch.receive()`: it is suspended while the receive waits, with no thread held, and resumed on
a worker by the send that serves it. A thread writes `ch.receive().wait()` and blocks
([README: Waiting operations](../README.md#waiting-operations)). A closed channel gives what was sent before the
close, then nothing at once.

## Parameters

None.

## Return value

An [operation](../operation.md) that gives, both by `co_await` in a task and by `.wait()` on a thread:

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

## Notes

`while (auto v = co_await ch.receive())` is the loop of a task over a channel, Go's `for v := range ch`; a thread
has the range-for ([begin](begin.md)) besides `while (auto v = ch.receive().wait())`. A debug build asserts on an
operation made and never carried out. `.wait()` is never written in a task, where it would hold a worker from every
other task.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> sum(async::channel<int> numbers) {
    int total = 0;
    while (auto n = co_await numbers.receive()) {  // suspended while the channel is empty
        total += *n;
    }
    co_return total;  // closed and drained
}

int main() {
    async::channel<int> numbers(4);
    async::task<int> summing = async::spawn(sum(numbers));
    for (int i : range(1, 11)) {
        numbers.send(i).wait();
    }
    numbers.close();
    println("{}", summing.wait());

    async::channel<int> left(2);
    left.send(7).wait();
    left.close();
    println("{}", left.receive().wait());
    println("{}", left.receive().wait());

    async::channel<void> done(1);
    done.send().wait();
    println("{}", done.receive().wait());
}
```

Output:

```text
55
7
nullopt
true
```

## See also

- [try_receive](try_receive.md): receives only what is there
- [on_receive](on_receive.md): a receive as a case of a select
- [begin](begin.md): the range-for of a thread
- [send](send.md), [close](close.md)
- [sgcl::async::channel\<T\>](../channel.md)
