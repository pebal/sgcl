[sgcl](../../README.md) › [hash](../README.md) › [adler32](../adler32.md)

# sgcl::hash::adler32::combine

```cpp
static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept;
```

The checksum of A followed by B, given `first`, the checksum of A, `second`, the checksum of B, and
`second_length`, the length of B in bytes: zlib's `adler32_combine`. With it pieces of a buffer hashed apart, on
several threads, are joined into the checksum of the whole.

Going through B from A's sums instead of from (1, 0) adds `a1 − 1` to every `a` on the way, so `a = a1 + a2 − 1`
and `b = b1 + b2 + n·(a1 − 1)`, modulo 65521. It is static, works on values, is `constexpr` and takes any length up
to 2^64 − 1.

## Parameters

| Parameter | Description |
|---|---|
| `first` | the checksum of the first piece |
| `second` | the checksum of the second piece |
| `second_length` | the length of the second piece in bytes |

## Return value

The checksum of the two pieces, the first followed by the second.

## Complexity

Constant: a few arithmetic operations, whatever `second_length`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    uint32_t a = hash::adler32::of("Wiki");
    uint32_t b = hash::adler32::of("pedia");
    println("{:08x}", hash::adler32::combine(a, b, 5));
    println("{:08x}", hash::adler32::of("Wikipedia"));
}
```

Output:

```text
11e60398
11e60398
```

## See also

- [crc32::combine](../crc32/combine.md): the same for a CRC, a buffer hashed on four tasks
- [resume](resume.md): going on from a saved checksum
- [sgcl::hash::adler32](../adler32.md)
