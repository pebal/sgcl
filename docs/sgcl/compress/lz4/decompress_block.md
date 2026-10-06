[sgcl](../../README.md) › [compress](../README.md) › [lz4](README.md)

# sgcl::compress::lz4::decompress_block

```cpp
static expected<vector<byte>, error> decompress_block(const slice<const byte>& data,                  // (1)
                                                      size_t max_size) noexcept;
static expected<vector<byte>, error> decompress_block(const slice<const byte>& data,                  // (2)
                                                      size_t max_size, const options& o) noexcept;
```

Decompresses one block of the LZ4 block format, with no frame: the block carries no size, so the caller gives the
most it may make, `max_size` (the size the caller kept beside the block, as `LZ4_decompress_safe`'s capacity), and a
block that makes more is `errc::too_large`. The result is the bytes the block made, at most `max_size` of them; the
memory taken is no more than a block can make, 255 bytes a byte of it, whatever `max_size` says.

1. No dictionary.
2. With the dictionary of the options (the only part read): its last 64 KB come before the data, as when the block
   was compressed.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the block |
| `max_size` | the most the block may decompress to |
| `o` | the dictionary ([options](../lz4-options.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): more than `max_size` (`errc::too_large`), a match before the
data or of offset 0 (`errc::corrupt`), a block that ends inside a sequence or after a match, or no block at all
(`errc::unexpected_end`).

## Complexity

Linear in the size of the output.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("a block without a frame, ").repeat(40);
    vector<byte> block = compress::lz4::compress_block(text);
    println("{}", compress::lz4::decompress_block(block, 1000)->size());
    println("{}", compress::lz4::decompress_block(block, 999).error().message());
}
```

Output:

```text
1000
offset 34: lz4: the block decodes to more than the size given
```

## See also

- [compress_block](compress_block.md): the other way
- [decompress](decompress.md): frames
- [sgcl::compress::lz4](README.md)
