[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::from_bytes

```cpp
static expected<hyperloglog, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Reads a sketch from the bytes of [to_bytes](to_bytes.md): the header and the size checked before the registers are
allocated, and every register checked against the most a 64-bit hash gives at the precision.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the bytes of a sketch |

## Return value

The sketch, or an [error](../error/README.md) with the byte it stopped on: `too short for the sketch's header`,
`not the sketch's magic`, `an unknown version of the sketch's format`, `a HyperLogLog's precision from 4 to 18`, `a
reserved byte of a HyperLogLog not zero`, `a HyperLogLog's size does not match its precision`, `a HyperLogLog's
register past the most a hash gives`.

## Complexity

Linear in the size of the input.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::hyperloglog h;
    for (uint64_t i = 0; i < 1000; ++i) {
        h.add(i);
    }
    auto back = concurrent::hyperloglog::from_bytes(h.to_bytes());
    println("{}", back->estimate() == h.estimate());
}
```

Output:

```text
true
```

## See also

- [to_bytes](to_bytes.md)
- [sgcl::concurrent::hyperloglog](README.md)
