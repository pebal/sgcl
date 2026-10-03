[sgcl](../../README.md) › [compress](../README.md) › [xz](README.md)

# sgcl::compress::xz::compress

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

Compresses the whole of the data at once into one `.xz` stream of one block, with the block's sizes in its header:
the data through the filters of the options into LZMA2, the check of the data, the index and the footer. Empty data
is a stream of no block (32 bytes), as xz makes it.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.
- (1), (3), (5) The default options: level 6, CRC-64, no filter.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the level, `-e`, the check, the filters, the dictionary ([options](../xz-options.md)) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data, by a factor the level sets.

## Exceptions

- (1), (3), (5) None.
- (2), (4), (6) `std::invalid_argument` when the options are out of range: a Delta distance past 256, a check that is not
  one of the four, a dictionary under 4 KiB or past 1.5 GiB, `level::huffman_only`.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    println("{}", compress::xz::compress(string()).size());
    println("{}", compress::xz::compress(string("x").repeat(1000)).size());
    try {
        compress::xz::compress("x", {.delta = 300});
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
32
76
compress::xz: a Delta distance of 1..256
```

## See also

- [decompress](decompress.md): the other way
- [xz::writer](../xz-writer/README.md): a stream
- [sgcl::compress::xz](README.md)
