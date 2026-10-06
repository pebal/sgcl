[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::value_to

```cpp
void value_to(const slice<byte>& out, uint64_t position = 0) const noexcept;
```

Writes `out.size()` bytes of the output, from byte `position` of it on: the extendable output of BLAKE3, of any
length, read from anywhere. The output is a run of 64-byte blocks, each the root compressed with its number, so
reading from a position costs nothing for the bytes before it. The hasher goes on: a read changes nothing, and two
reads of the same position give the same bytes (where SHAKE's [read](../shake256/read.md) goes on where the last one
stopped and closes the input). The first 32 bytes from position 0 are [value](value.md). With nothing to write it
writes nothing.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the bytes go, as many as it holds: a `vector<byte>`, an `array`, a `secret_bytes` for a key |
| `position` | the byte of the output to start at, 0 by default |

## Return value

None.

## Complexity

Linear in `out.size()`: one compression for 64 bytes, four at a time on the vector units for a long output.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::blake3 h;
    h.update("abc");
    array<byte, 16> middle;
    h.value_to(middle, 32);  // bytes 32 to 47 of the output
    println(encoding::hex::encode(middle));
}
```

Output:

```text
1fb250ae7393f5d02813b65d521a0d49
```

## See also

- [value](value.md): the first 32 bytes
- [derive_key](derive_key.md): output of any length as a key
- [sgcl::crypto::blake3](README.md)
