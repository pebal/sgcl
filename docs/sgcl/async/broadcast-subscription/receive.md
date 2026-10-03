[sgcl](../../README.md) › [async](../README.md) › [broadcast](../broadcast/README.md) › [subscription](README.md)

# sgcl::async::broadcast\<T\>::subscription::receive

```cpp
auto receive() noexcept;
```

Receives the next value, waiting for one. The call makes an [operation](../operation/README.md) that does nothing yet;
carried out, the receive copies the value at the cursor out of the ring and moves the cursor on, or waits until a
sender commits the position or the broadcast is closed. A cursor the ring has lapped moves to the oldest value still
there, and [lagged](lagged.md) counts the ones it passed over.

A task writes `co_await s.receive()`: it registers the position with its frame and is suspended, with no thread held,
until the sender that commits the position hands it to the scheduler. A thread writes `s.receive().wait()`: it looks
for the commit for a few microseconds, since a sender at work is nanoseconds from it, then parks until the sender
wakes it ([README: Waiting operations](../README.md#waiting-operations)).

## Parameters

None.

## Return value

An [operation](../operation/README.md) that gives an `optional<T>`, both by `co_await` in a task and by `.wait()` on a
thread: a copy of the next value, or `nullopt` once the broadcast is closed and every value sent before the close
was received.

## Complexity

Constant when the value is there: a copy and an atomic decrement on the value's node. A wait allocates nothing.

## Exceptions

- The call: none.
- Carried out: what the copy constructor, the move constructor and the move assignment of `T` throw. A receive wakes
  nobody.

## Notes

The subscription must not be empty, and is read by one thread or task at a time. A debug build asserts on an
operation made and never carried out. `.wait()` is never written in a task, where it would hold a worker from every
other task.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> sum(async::broadcast<int>::subscription numbers) {
    int total = 0;
    while (auto n = co_await numbers.receive()) {  // suspended until the next send
        total += *n;
    }
    co_return total;
}

int main() {
    async::broadcast<int> numbers(16);
    async::task<int> summing = async::spawn(sum(numbers.subscribe()));
    auto mine = numbers.subscribe();
    for (int i : range(1, 6)) {
        numbers.send(i);
    }
    numbers.close();

    println("{}", summing.wait());
    while (auto n = mine.receive().wait()) {
        print("{} ", *n);
    }
    println("{}", mine.receive().wait());
}
```

Output:

```text
15
1 2 3 4 5 nullopt
```

## See also

- [try_receive](try_receive.md): receives only what is there
- [on_receive](on_receive.md): a receive as a case of a select
- [lagged](lagged.md): the values lost before the one received
- [sgcl::async::broadcast\<T\>::subscription](README.md)
