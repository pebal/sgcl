[sgcl](../../README.md) › [compress](../README.md) › [lz4](README.md)

# sgcl::compress::lz4::compress

```cpp
static vector<byte> compress(const slice<const byte>& data) noexcept;             // (1)
static vector<byte> compress(const slice<const byte>& data, const options& o);    // (2)
static vector<byte> compress(const string& text) noexcept;                        // (3)
static vector<byte> compress(const string& text, const options& o);               // (4)
template<class T>
static vector<byte> compress(const T& text) noexcept;                             // (5)
template<class T>
static vector<byte> compress(const T& text, const options& o);                    // (6)
```

Compresses the whole of the data at once into one LZ4 frame: the header with the content's size, the blocks of the
options' size, each compressed or, where that would not make it smaller, stored as it is, the end mark and the
checksums asked for. Without a dictionary the blocks are compressed straight from the data, a linked block's history
the data before it; the tables of the compressor are the thread's, kept from one call to the next, so a small
compress allocates only its result. Empty data is a frame of no block.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.
- (1), (3), (5) The default options: level 1, blocks of 4 MB, independent, the content's checksum.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the level, the block size and kind, the checksums, a dictionary ([options](../lz4-options.md)) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data, by a factor the level sets.

## Exceptions

- (1), (3), (5) None.
- (2), (4), (6) `std::invalid_argument` when the block size is not one of the four.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    println("{}", compress::lz4::compress(string()).size());
    println("{}", compress::lz4::compress(string("x").repeat(1000)).size());
    println("{}", compress::lz4::compress(string("x").repeat(1000), {.block_checksum = true}).size());
    try {
        compress::lz4::compress("x", {.block_size = compress::lz4::block_size(2)});
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
23
41
45
compress::lz4: a block size of kb64, kb256, mb1 or mb4
```

## See also

- [decompress](decompress.md): the other way
- [compress_block](compress_block.md): no frame
- [lz4::writer](../lz4-writer/README.md): a stream
- [sgcl::compress::lz4](README.md)
