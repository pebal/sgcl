[sgcl](../../README.md) › [async](../README.md) › [receive_channel](README.md)

# sgcl::async::receive_channel\<T\>::receive_channel

```cpp
receive_channel(const channel<T>& ch) noexcept;                // (1)
receive_channel(const receive_channel&) noexcept = default;    // (2)
receive_channel(receive_channel&&) noexcept = default;         // (3)
```

Makes the receiving end of a channel, or another handle of one.

1. The receiving end of the channel `ch` refers to: the same state, so `==` with `ch` gives `true`. Not explicit: a
   `channel<T>` converts to a `receive_channel<T>` where one is expected, a parameter of a function that only
   receives.
2. A handle of the same channel: the copies share the state.
3. The same as (2): a move of the handle's word is a copy, and the source still refers to the channel.

There is no constructor of a new channel and no empty one: a channel is made by [channel](../channel/channel.md)'s
constructor. The constructors of `receive_channel<void>` are the same, from a `channel<void>`.

## Parameters

| Parameter | Description |
|---|---|
| `ch` | the channel whose receiving end to make |
| `const receive_channel&`, `receive_channel&&` | the handle to copy |

## Complexity

Constant: a copy of one tracked word.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(2);
    async::receive_channel<int> in = numbers;
    async::receive_channel<int> copy = in;
    numbers.send(5).wait();
    println("{}", *copy.receive().wait());
    println("{} {}", in == numbers, copy == in);
}
```

Output:

```text
5
true true
```

## See also

- [operator=](operator_assign.md): makes the handle one of another channel's receiving end
- [channel](../channel/channel.md): makes a channel
- [sgcl::async::receive_channel\<T\>](README.md)
