[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::receive_channel\<T\>::begin

```cpp
iterator begin() const noexcept;
```

Returns an iterator at the start of a range-for over the channel, the channel's [begin](../channel/begin.md): it
receives an element, waiting for it as `receive().wait()` does, at the first look at it (`*it`, `it->` or the
comparison with [end](end.md)), and `++` only marks it used, so that a `break` or `std::views::take` leaves every
element it did not look at in the channel. Once the channel is closed and drained, the iterator compares equal to
[end](end.md). The iterator is a `std::input_iterator`, and the views of `std::ranges` take the channel.

`receive_channel<void>` has no `begin`.

## Parameters

None.

## Return value

An iterator at the first element, which it has not received yet.

## Complexity

Constant. A look at an element costs a receive: constant when the element is there, and a wait for it when not.

## Exceptions

None. A look at an element (`*it`, `it->`, the comparison with the end) throws what a receive throws:

- `std::system_error` when the receive wakes a waiting sender's task and the wake starts the scheduler's workers,
  one of which cannot be started.
- What the move constructor of `T` throws.

A move of `T` that throws loses the element being moved and leaves the channel otherwise as it was: its slot
of the buffer is free for the sends after it. A receive that waits throws what the move of its element into it
threw, made by the send that served it.

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

- [end](end.md): the end of a range-for
- [receive](receive.md): one receive
- [sgcl::async::receive_channel\<T\>](README.md)
