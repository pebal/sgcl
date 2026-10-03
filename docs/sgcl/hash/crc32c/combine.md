[sgcl](../../README.md) › [hash](../README.md) › [crc32c](../crc32c.md)

# sgcl::hash::crc32c::combine

```cpp
static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept;
```

The CRC of A followed by B, given `first`, the CRC of A, `second`, the CRC of B, and `second_length`, the length of
B in bytes: zlib's `crc32_combine`, here for every CRC. With it a large buffer is hashed in pieces on several
threads or tasks and the pieces' CRCs joined, as pigz and a zip written from many threads do.

It is static and works on values: a CRC does not count its own length, which would cost every `update` an addition
for the sake of a call made once per piece. It is `constexpr`, and takes any length up to 2^64 − 1.

## Parameters

| Parameter | Description |
|---|---|
| `first` | the CRC of the first piece |
| `second` | the CRC of the second piece |
| `second_length` | the length of the second piece in bytes |

## Return value

The CRC of the two pieces, the first followed by the second.

## Complexity

Logarithmic in `second_length`: one multiplication modulo the polynomial for every bit of it that is set, over a
table of powers the compiler computes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    uint32_t first = hash::crc32c::of("hello, ");
    uint32_t second = hash::crc32c::of("world");
    println("{:08x}", hash::crc32c::combine(first, second, 5));
    println("{:08x}", hash::crc32c::of("hello, world"));
}
```

Output:

```text
6999a41f
6999a41f
```

## See also

- [crc32::combine](../crc32/combine.md): a buffer hashed on four tasks
- [resume](resume.md): going on from a saved CRC
- [sgcl::hash::crc32c](../crc32c.md)
