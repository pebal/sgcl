[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::from_bytes

```cpp
static expected<bloom_filter, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Reads a filter from the bytes of [to_bytes](to_bytes.md). Every field is checked, and the size of the input against
the bits it declares, before the bits are allocated: a declared *m* that the input does not hold is an error, not an
allocation.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the bytes of a filter |

## Return value

The filter, or an [error](../error/README.md) with the byte it stopped on: `too short for the sketch's header`, `not
the sketch's magic`, `an unknown version of the sketch's format`, `a Bloom filter's hashes from 1 to 64`, `a reserved
byte of a Bloom filter not zero`, `a Bloom filter's bits not a positive multiple of 64`, `a Bloom filter's size does
not match its bits`.

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
    concurrent::bloom_filter f(100);
    f.add("kept");
    auto back = concurrent::bloom_filter::from_bytes(f.to_bytes());
    println("{}", back->contains("kept"));
    vector<byte> junk(20);
    println("{}", concurrent::bloom_filter::from_bytes(junk).error().message());
}
```

Output:

```text
true
not the sketch's magic
```

## See also

- [to_bytes](to_bytes.md)
- [sgcl::concurrent::bloom_filter](README.md)
