[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md)

# sgcl::compress::lzma::compress

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

Compresses the whole of the data at once into an `.lzma` stream: the header of 13 bytes with the size of the data
in it, and the range coder's data with no end marker, as the LZMA SDK writes it. The encoder's tables are sized to
the data when it is smaller than the dictionary, and the data is coded where it lies.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.
- (1), (3), (5) The default options: level 6, a dictionary of 8 MiB.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the level, `-e`, the dictionary, lc, lp and pb ([options](../lzma-options.md)) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data, by a factor the level sets.

## Exceptions

- (1), (3), (5) None.
- (2), (4), (6) `std::invalid_argument` when the options are out of range: lc past 8, lp or pb past 4, a dictionary
  under 4 KiB or past 1.5 GiB, `level::huffman_only`.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(100);
    println("{}", compress::lzma::compress(text).size());
    try {
        compress::lzma::compress(text, {.lc = 9});
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
76
compress::lzma: lc 0..8, lp 0..4, pb 0..4
```

## See also

- [decompress](decompress.md): the other way
- [lzma::writer](../lzma-writer.md): a stream, with the end marker
- [sgcl::compress::lzma](../lzma.md)
