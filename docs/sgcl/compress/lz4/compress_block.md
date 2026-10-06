[sgcl](../../README.md) › [compress](../README.md) › [lz4](README.md)

# sgcl::compress::lz4::compress_block

```cpp
static vector<byte> compress_block(const slice<const byte>& data) noexcept;                      // (1)
static vector<byte> compress_block(const slice<const byte>& data, const options& o) noexcept;    // (2)
```

Compresses the data into one block of the LZ4 block format, with no frame around it: no header, no checksum, no
size, what `LZ4_compress_default` and `LZ4_compress_HC` make. It is what a format that keeps the size elsewhere
carries (a database page, a record batch, a game's asset table); the reader of the block needs that size, or a bound
on it, to call [decompress_block](decompress_block.md). The block's last five bytes are literals and no match starts
in its last twelve, as every decoder of the format expects.

1. Level 1, no dictionary.
2. The level and the dictionary of the options; the frame's fields are not read. With a dictionary, its last 64 KB
   come before the data, as if decoded before it.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `o` | the level and a dictionary ([options](../lz4-options.md)) |

## Return value

The block, a new vector: at most the data's size and a byte in 255 more, and 16.

## Complexity

Linear in the size of the data, by a factor the level sets.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("a block without a frame, ").repeat(40);
    vector<byte> block = compress::lz4::compress_block(text, {.level = 9});
    println("{} bytes into {}", text.size(), block.size());
    auto back = compress::lz4::decompress_block(block, text.size());
    println("{}", back->size());
}
```

Output:

```text
1000 bytes into 39
1000
```

## See also

- [decompress_block](decompress_block.md): the other way
- [compress](compress.md): a frame
- [sgcl::compress::lz4](README.md)
