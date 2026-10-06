[sgcl](../../README.md) › [compress](../README.md) › [snappy](README.md)

# sgcl::compress::snappy::decompressed_size

```cpp
static expected<uint64_t, error> decompressed_size(const slice<const byte>& block) noexcept;
```

Reads the length a block of Snappy's block format decompresses to, from the varint at its head, without decompressing
anything (`snappy::GetUncompressedLength`): what a program sizes its buffer by or holds against its own bound.

## Parameters

| Parameter | Description |
|---|---|
| `block` | the block, or its first five bytes at least |

## Return value

The length, below 2^32, or the [error](../error/README.md): a head cut short (`errc::unexpected_end`), a varint longer
than five bytes or past 2^32 (`errc::corrupt`).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> block = compress::snappy::compress_block(string("x").repeat(300));
    println("{} bytes in, {} out", block.size(), *compress::snappy::decompressed_size(block));
    println("{}", compress::snappy::decompressed_size(slice<const byte>()).error().message());
}
```

Output:

```text
19 bytes in, 300 out
offset 0: snappy: a block's length that cannot be read
```

## See also

- [decompress_block](decompress_block.md)
- [sgcl::compress::snappy](README.md)
