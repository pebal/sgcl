[sgcl](../../README.md) › [async](../README.md) › [channel](README.md)

# sgcl::async::channel\<T\>::channel

```cpp
channel() noexcept;                        // (1)
explicit channel(size_type capacity);      // (2)
channel(const channel& other) noexcept;    // (3)
channel(channel&& other) noexcept;         // (4)
```

Makes a channel, or another handle of an existing one.

1. A rendezvous: a channel of capacity 0, whose send waits until a receive takes the element.
2. A channel that buffers `capacity` elements; a `capacity` of 0 is a rendezvous.
3. A handle of the channel `other` is: the copies share the state, and `==` gives `true` for them.
4. The same as (3): a move of the handle's word is a copy, and `other` still refers to the channel.

The state is made on the managed heap with its ring and the first nodes of its two lists of waiters. A ring of up to
8 slots of at most 64 bytes, which a rendezvous and every capacity up to 8 of small elements have, lies in the state
itself; a larger one is an array of its own, a power of two of slots at least `capacity`. The constructors of
`channel<void>` are the same.

## Parameters

| Parameter | Description |
|---|---|
| `capacity` | the number of elements the channel buffers; 0 for a rendezvous |
| `other` | the handle of the channel to refer to |

## Complexity

- (1) Constant: one managed object.
- (2) Linear in `capacity`: the slots of the ring are made, at least 8.
- (3–4) Constant.

## Exceptions

- (1), (3–4) None.
- (2) `length_error` when the ring of `capacity` elements would be larger than the largest array.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::channel<int> meeting;      // a rendezvous
    async::channel<int> buffered(8);
    async::channel<int> same = buffered;

    println("{} {}", meeting.capacity(), buffered.capacity());
    same.send(1).wait();
    println("{} {}", buffered.size(), same == buffered);

    async::channel<void> signals(1);  // a channel of signals
    println("{}", signals.try_send());
}
```

Output:

```text
0 8
1 true
true
```

## See also

- [operator=](operator_assign.md): makes the handle one of another channel
- [capacity](capacity.md): the capacity given here
- [sgcl::async::channel\<T\>](README.md)
