[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::begin

```cpp
iterator begin() const;
```

Receives the first element, waiting for it as `receive().wait()` does, and returns an iterator that holds it: the
start of a range-for over the channel, the channel's [begin](../channel/begin.md). Each `++` receives the next
element the same way, and the iterator becomes the [end](end.md) iterator once the channel is closed and drained.

`receive_channel<void>` has no `begin`.

## Parameters

None.

## Return value

An iterator holding the first element, or the end iterator when the channel is closed and drained.

## Complexity

That of a receive: constant when the element is there, and a wait for it when not.

## Exceptions

- `std::system_error` when the receive wakes a waiting sender's task and the wake starts the scheduler's workers,
  one of which cannot be started.
- What the move constructor of `T` throws.

A move of `T` that throws loses the element being moved and leaves the channel otherwise as it was: its slot
of the buffer is free for the sends after it. A receive that waits throws what the move of its element into it
threw, made by the send that served it.

The same for every `++`.

## Notes

The range-for blocks the calling thread at every element: it is a thread's loop, never a task's. A task writes
`while (auto v = co_await in.receive())` ([receive](receive.md)).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int total(async::receive_channel<int> in) {
    int sum = 0;
    for (int v : in) {
        sum += v;
    }
    return sum;
}

int main() {
    async::channel<int> numbers(3);
    numbers.send(1).wait();
    numbers.send(2).wait();
    numbers.send(3).wait();
    numbers.close();
    println("{}", total(numbers));
}
```

Output:

```text
6
```

## See also

- [end](end.md): the end iterator
- [receive](receive.md): one receive
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
