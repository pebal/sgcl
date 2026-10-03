[sgcl](../../README.md) › [async](../README.md) › [channel](../channel.md)

# sgcl::async::channel\<T\>::try_send

```cpp
bool try_send(const T& value) const;    // (1)
bool try_send(T&& value) const;         // (2)
bool try_send() const;                  // (3)
```

Sends an element if it can without waiting: to a waiting receiver when the buffer is empty, or into the buffer when
it has room. A rendezvous has no buffer, so there a `try_send` succeeds only when a receiver waits.

1. Sends a copy of `value`.
2. Sends `value`, moved out of it only when it is delivered.
3. `channel<void>`: sends a signal.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the element to send |

## Return value

`true` when the element was delivered; `false` when the channel is closed, when the buffer is full, or, on a
rendezvous, when no receiver waits.

## Complexity

Constant: one compare-exchange on the ring, or the take of a waiting receiver; plus its wake.

## Exceptions

- `std::system_error` when the send wakes a waiting task and the wake starts the scheduler's workers, one of which
  cannot be started.
- What the copy (1) or the move constructor of `T` throws.

A move of `T` that throws leaves the channel as it was: the element is not delivered, a receiver it was
being handed to waits on, and a slot of the buffer it was being moved into is passed over by the receives.

## Notes

(2) moves the element only when it delivers it: a `false`, whether the channel is closed, the buffer is full or no
receiver waits, leaves `value` as it was, so a move-only element can be kept or sent another way.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> numbers(2);
    for (int i : range(1, 4)) {
        println("{}: {}", i, numbers.try_send(i));
    }
    println("{} in the buffer", numbers.size());

    async::channel<int> meeting;  // a rendezvous, nobody receiving
    println("{}", meeting.try_send(1));

    numbers.close();
    println("{}", numbers.try_send(4));

    async::channel<void> signals(1);
    bool first = signals.try_send();
    bool second = signals.try_send();
    println("{} {}", first, second);
}
```

Output:

```text
1: true
2: true
3: false
2 in the buffer
false
false
true false
```

## See also

- [send](send.md): waits for room or for a receiver
- [on_send](on_send.md): a send as a case of a select, with `otherwise` for a poll
- [try_receive](try_receive.md): receives without waiting
- [sgcl::async::channel\<T\>](../channel.md)
