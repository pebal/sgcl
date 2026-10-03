[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::try_receive

```cpp
/*(1)*/ optional<T> try_receive() const;
/*(2)*/ bool try_receive() const;  // receive_channel<void>
```

Receives an element if one is there, without waiting: the channel's [try_receive](../channel/try_receive.md).

1. Receives an element.
2. `receive_channel<void>`: receives a signal.

## Parameters

None.

## Return value

- (1) The element, or `nullopt` when there was none.
- (2) `true` when a signal was there, `false` otherwise.

## Complexity

Constant: one compare-exchange on the ring, plus the wake of the sender whose element it moved in.

## Exceptions

- `std::system_error` when the receive wakes a waiting sender's task and the wake starts the scheduler's workers,
  one of which cannot be started.
- What the move constructor of `T` throws.

A move of `T` that throws loses the element being moved and leaves the channel otherwise as it was: its slot
of the buffer is free for the sends after it.

## Notes

`nullopt` does not tell an empty channel from a closed one: [closed](closed.md) does.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(2);
    async::receive_channel<int> in = numbers;
    println("{}", in.try_receive());
    numbers.send(3).wait();
    println("{}", in.try_receive());
}
```

Output:

```text
nullopt
3
```

## See also

- [receive](receive.md): waits for an element
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
