[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::try_receive

```cpp
optional<T> try_receive() const;    // (1)
bool try_receive() const;           // (2), channel<void>
```

Receives an element if one is there, without waiting: the first element of the buffer, with the first waiting
sender's element moved in behind it, or, when the buffer is empty, a waiting sender's element. On a rendezvous an
element is there only when a sender waits.

1. Receives an element.
2. `channel<void>`: receives a signal.

## Parameters

None.

## Return value

- (1) The element, or `nullopt` when there was none. `optional` is the alias of `std::optional`
  ([aliases](../../core/aliases.md)).
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

`nullopt` does not tell an empty channel from a closed one: [closed](closed.md) does, and a channel closed with
elements in it still gives them.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<string> words(4);
    words.send("first").wait();
    words.send("second").wait();

    while (auto w = words.try_receive()) {
        println("{}", *w);
    }
    println("{}", words.try_receive());

    async::channel<void> signals(1);
    signals.send().wait();
    bool first = signals.try_receive();
    bool second = signals.try_receive();
    println("{} {}", first, second);
}
```

Output:

```text
first
second
nullopt
true false
```

## See also

- [receive](receive.md): waits for an element
- [on_receive](on_receive.md): with `otherwise`, a receive in a select that never waits
- [try_send](try_send.md): sends without waiting
- [sgcl::async::channel\<T\>](../channel.md)
