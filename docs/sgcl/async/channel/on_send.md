[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::on_send

```cpp
/*(1)*/ template<class F = void (*)()>
        auto on_send(const T& value, F f = [] {}) const
            noexcept(std::is_nothrow_copy_constructible_v<T> &&
                     std::is_nothrow_move_constructible_v<T> &&
                     std::is_nothrow_move_constructible_v<F>);
/*(2)*/ template<class F = void (*)()>
        auto on_send(T&& value, F f = [] {}) const
            noexcept(std::is_nothrow_move_constructible_v<T> &&
                     std::is_nothrow_move_constructible_v<F>);
/*(3)*/ template<class F = void (*)()>  // channel<void>
        auto on_send(F f = [] {}) const noexcept(std::is_nothrow_move_constructible_v<F>);
```

A send as a case of a [select](../select.md): the case holds the element and is served when the select delivers it
to this channel, to a waiting receiver or into the buffer; its body `f()` is called then, after the wait. A closed
channel serves the case at once, without the body: the element is not delivered, and the index the select gives
says which case it was.

1. The case sends a copy of `value`.
2. The case sends `value`, moved.
3. `channel<void>`: the case sends a signal.

While the select waits, the case's element is with the channel's waiting senders, served in its order like any
waiting sender's; a select that does not take the case takes the element back.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to send |
| `f` | the body of the case, `void()`; by default one that does nothing |

## Return value

The case: a `send_case<F>` (1–2), a send case over the channel of signals (3), to be passed to
[select](../select.md) by value. Nothing is sent until the select is carried out, and a case is for one select.

## Complexity

Constant: the element and `f` are moved into the case.

## Exceptions

What the copy (1) or the move constructor of `T` throws, and what the move constructor of `F` throws; none when they
are noexcept. What the body throws when the case is served comes out of the select.

## Notes

A select with a send case needs `T` to be move-assignable too: the element is moved into the case's waiter and, when
the select takes the case back, moved back. A select must not have a send case and a receive case on one channel,
which would each find the other's waiter and wait for itself (Go's blocks); a debug build asserts.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> fast(1), slow(1);
    slow.send(0).wait();  // full

    size_t served = async::select(
        fast.on_send(1, [] { println("to fast"); }),
        slow.on_send(1, [] { println("to slow"); })).wait();
    println("case {}", served);

    async::select(fast.on_send(2), async::otherwise([] { println("both full"); })).wait();

    fast.close();
    served = async::select(fast.on_send(3, [] { println("sent"); })).wait();
    println("case {}, closed", served);
}
```

Output:

```text
to fast
case 0
both full
case 0, closed
```

## See also

- [on_receive](on_receive.md): a receive as a case
- [select](../select.md), [otherwise](../otherwise.md): the wait over the cases, and the case taken at once
- [send](send.md): the send alone
- [sgcl::async::channel\<T\>](../channel.md)
