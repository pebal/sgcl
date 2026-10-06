[sgcl](../../README.md) › [compress](../README.md) › [snappy](README.md)

# sgcl::compress::snappy::compress_block

```cpp
static vector<byte> compress_block(const slice<const byte>& data) noexcept;    // (1)
static vector<byte> compress_block(const string& text) noexcept;               // (2)
template<class T>
static vector<byte> compress_block(const T& text) noexcept;                    // (3)
```

Compresses the data into one block of Snappy's block format, what `snappy::Compress` makes: the length as a varint,
then the elements, the data cut into fragments of 64 KB compressed each on its own. No checksum: a format that
carries the block checks it, or the [framing format](compress.md) does.

1. The bytes of `data`.
2. The bytes of the text.
3. Takes part only for a literal, a character array or a `std::string_view`: the text's bytes, as (2) takes them.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |

## Return value

The block, a new vector: at most the data's size, a sixth of it more, and 32 bytes.

## Complexity

Linear in the size of the data.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("a block without framing, ").repeat(40);
    vector<byte> block = compress::snappy::compress_block(text);
    println("{} bytes into {}", text.size(), block.size());
    println("{}", compress::snappy::decompress_block(block)->size());
}
```

Output:

```text
1000 bytes into 76
1000
```

## See also

- [decompress_block](decompress_block.md): the other way
- [compress](compress.md): the framing format
- [sgcl::compress::snappy](README.md)
