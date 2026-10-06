[sgcl](../../README.md) › [compress](../README.md) › [bzip2](README.md)

# sgcl::compress::bzip2::compress

```cpp
static vector<byte> compress(const slice<const byte>& data) noexcept;                      // (1)
static vector<byte> compress(const slice<const byte>& data, const options& o) noexcept;    // (2)
static vector<byte> compress(const string& text) noexcept;                                 // (3)
static vector<byte> compress(const string& text, const options& o) noexcept;               // (4)
template<class T>
static vector<byte> compress(const T& text) noexcept;                                      // (5)
template<class T>
static vector<byte> compress(const T& text, const options& o) noexcept;                    // (6)
```

Compresses the whole of the data at once into one bzip2 stream, as `bzip2 -N` writes it: `BZh` and the level, the
blocks — each the data after the first run-length pass, up to 100 KB × level less 19 bytes, sorted by the
Burrows-Wheeler transform, moved to front, its runs of zeros in base 2, coded by two to six Huffman tables — with the
CRC of each block's bytes, and the end of the stream with the CRC of the blocks' CRCs. Empty data is a stream of no
block, 14 bytes.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.
- (1), (3), (5) The default level, 9: blocks of 900 KB.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the level ([options](../bzip2-options.md)) |

## Return value

The compressed bytes, a new vector.

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
    string text = string("the same words, the same words, the same words again. ").repeat(100);
    println("{}", compress::bzip2::compress(string()).size());
    println("{}", compress::bzip2::compress(text).size());
    println("{}", compress::bzip2::compress(text, {.level = 1}).size());
}
```

Output:

```text
14
91
91
```

## See also

- [decompress](decompress.md): the other way
- [bzip2::writer](../bzip2-writer/README.md): a stream
- [sgcl::compress::bzip2](README.md)
