[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::on_receive

```cpp
template<class F>
auto on_receive(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);    // (1)
template<class F>  // channel<void>
auto on_receive(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);    // (2)
```

A receive as a case of a [select](../select.md): the case is served when the select receives an element from this
channel, and its body `f` is called then, after the wait.

1. `f` is called with the element. A body that takes `optional<T>` (one callable with it) is also called with
   `nullopt` when the channel is closed and drained; a body that takes `T` is called only with an element, and a
   case served by the close calls nothing.
2. `channel<void>`: `f()` is called for a signal and for the close alike, as both end a wait for a signal.

A closed channel serves the case at once: with what was sent before the close, then with nothing. The select gives
the index of the case in every event, so a body that takes `T` and the index together tell an element from the
close.

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

## Notes

The body runs on the thread that called the select or on the worker that resumed the task, never inside a channel
operation: it may do anything, another select included. A task's body is a plain function: a `co_await` belongs in
the task after the select, not in the body.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(4);
    async::channel<string> words(4);
    auto receive_one = [&] {
        return async::select(
            numbers.on_receive([](int n) { println("number {}", n); }),
            words.on_receive([](optional<string> w) { println("word {}", w); })).wait();
    };

    numbers.send(7).wait();
    println("case {}", receive_one());
    words.send("hi").wait();
    println("case {}", receive_one());
    words.close();
    println("case {}", receive_one());  // served by the close

    async::channel<void> quit;
    quit.close();
    async::select(quit.on_receive([] { println("quit"); })).wait();
}
```

Output:

```text
number 7
case 0
word "hi"
case 1
word nullopt
case 1
quit
```

## See also

- [on_send](on_send.md): a send as a case
- [select](../select.md), [otherwise](../otherwise.md): the wait over the cases, and the case taken at once
- [receive](receive.md): the receive alone
- [sgcl::async::channel\<T\>](README.md)
