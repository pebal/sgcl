[sgcl](../../README.md) › [math](../README.md) › [random](../random.md)

# sgcl::math::random::random

```cpp
random();                                   // (1)
explicit random(uint64_t seed) noexcept;    // (2)
```

Constructs a generator.

1. Unpredictable: a new stream, its key four draws of a generator of the thread. That generator takes its own key
   from the system (`std::random_device`) the first time the thread asks, and again in the child of a `fork()`, so
   two defaults never repeat each other, in two threads or across a fork.
2. Repeatable: the same numbers in every run and on every platform. The key is `seed` in eight bytes,
   little-endian, followed by 24 zero bytes: the stream of Go's `rand.NewChaCha8` of that key, so the `Uint64`s,
   the `Int64N`, the `Float64`, the `Shuffle` and the `Perm` Go draws from it are what this draws.

A `random` is trivially copyable: a copy copies the stream, and the two draw the same numbers after.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | the first eight bytes of the key, little-endian |

## Complexity

- (1) A few draws of the thread's generator and a block of ChaCha8; a call to the system only the first time in a
  thread, or the first time after a `fork()`.
- (2) A block of ChaCha8.

## Exceptions

- (1) What `std::random_device` throws when the system has no source of randomness to give.
- (2) None.

## Notes

Making a default `random` inside a hot loop is the wrong shape: make it once and ask it many times.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random replay(42);
    math::random again(42);
    println("{} {}", replay.next_uint64(), again.next_uint64());

    math::random copy = replay;
    println("{}", copy.next_int(1000) == replay.next_int(1000));

    math::random fresh;
    math::random other;
    println("{}", fresh.next_uint64() == other.next_uint64());
}
```

Output:

```text
15742378508252295202 15742378508252295202
true
false
```

## See also

- [next_uint64](next_uint64.md): the words of the stream
- [sgcl::math::random](../random.md)
