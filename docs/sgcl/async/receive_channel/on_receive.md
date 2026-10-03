[sgcl](../../README.md) › [async](../README.md) › [receive_channel](../receive_channel.md)

# sgcl::async::receive_channel\<T\>::on_receive

```cpp
template<class F>
auto on_receive(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);    // (1)
template<class F>  // receive_channel<void>
auto on_receive(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);    // (2)
```

A receive as a case of a [select](../select.md): the channel's [on_receive](../channel/on_receive.md), the same
case. It is served when the select receives an element from the channel, and its body `f` is called then, after the
wait.

1. `f` is called with the element; a body that takes `optional<T>` is also called with `nullopt` when the channel
   is closed and drained, a body that takes `T` only with an element.
2. `receive_channel<void>`: `f()` is called for a signal and for the close alike.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case: `void(T)` or `void(optional<T>)` (1), `void()` (2) |

## Return value

The case: a `receive_case<F>` (1), a receive case over the channel of signals (2), to be passed to
[select](../select.md) by value. Nothing is received until the select is carried out, and a case is for one
select.

## Complexity

Constant: `f` is moved into the case.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. What the body throws when the case is served
comes out of the select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(2);
    async::stop_source source;
    async::receive_channel<int> in = numbers;
    async::receive_channel<void> stop = source.token().channel();
    auto next = [&] {
        async::select(in.on_receive([](int n) { println("got {}", n); }),
                      stop.on_receive([] { println("stopped"); }))
            .wait();
    };
    numbers.send(4).wait();
    next();
    source.request_stop();
    next();
}
```

Output:

```text
got 4
stopped
```

## See also

- [receive](receive.md): the receive as an operation
- [select](../select.md): the wait on several cases
- [sgcl::async::receive_channel\<T\>](../receive_channel.md)
