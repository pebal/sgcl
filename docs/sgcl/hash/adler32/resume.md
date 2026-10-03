[sgcl](../../README.md) › [hash](../README.md) › [adler32](../adler32.md)

# sgcl::hash::adler32::resume

```cpp
static adler32 resume(uint32_t value) noexcept;
```

Makes a hasher that goes on from `value`, the checksum of the bytes that came before: what it hashes then continues
those bytes, and its [value](value.md) is the checksum of them all. Go restores a saved state through
`UnmarshalBinary`; here the checksum is the whole state.

A value whose halves are 65521 or more is not a checksum: each half is taken modulo 65521, so that the sums never
start above what the update's bound allows. A value is not a seed: `adler32::of(data, v)` does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the checksum of the bytes before, as `value()` gave it |

## Return value

A hasher whose `value()` is `value`, each half taken modulo 65521, and whose updates go on from it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    uint32_t saved = hash::adler32::of("Wiki");
    auto h = hash::adler32::resume(saved);
    h.update("pedia");
    println("{:08x}", h.value());
    println("{}", h.value() == hash::adler32::of("Wikipedia"));

    // no checksum: each half is taken modulo 65521
    println("{:08x}", hash::adler32::resume(0xffffffff).value());
}
```

Output:

```text
11e60398
true
000e000e
```

## See also

- [combine](combine.md): the checksum of two pieces from their checksums
- [(constructor)](adler32.md): a hasher of no bytes yet
- [sgcl::hash::adler32](../adler32.md)
