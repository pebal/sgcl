[sgcl](../../README.md) › [hash](../README.md) › [crc64](README.md)

# sgcl::hash::crc64::resume

```cpp
static crc64 resume(uint64_t value) noexcept;
```

Makes a hasher that goes on from `value`, the CRC of the bytes that came before: what it hashes then continues
those bytes, and its [value](value.md) is the CRC of them all. Go's `crc64.Update(crc, tab, p)` is `resume(crc)`
followed by `update(p)`.

A value is not a seed: a one-argument constructor of the module ([xxh3_64](../xxh3_64/README.md),
[maphash](../maphash/README.md)) is a seed, and a CRC goes on from a value only through `resume`; `crc64::of(data, v)`
does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the CRC of the bytes before, as `value()` gave it |

## Return value

A hasher whose `value()` is `value` and whose updates go on from it.

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
    // the CRC of a log kept beside it, extended when a line is appended
    uint64_t saved = hash::crc64::of("first line\n");
    auto h = hash::crc64::resume(saved);
    println("{}", h.value() == saved);
    h.update("second line\n");
    println("{:016x}", h.value());
    println("{}", h.value() == hash::crc64::of("first line\nsecond line\n"));
}
```

Output:

```text
true
64daa2b2ef31e912
true
```

## See also

- [combine](combine.md): the CRC of two pieces from their CRCs
- [(constructor)](crc64.md): a hasher of no bytes yet
- [sgcl::hash::crc64](README.md)
