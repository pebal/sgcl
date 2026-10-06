[sgcl](../../README.md) › [compress](../README.md) › [zstd](README.md)

# sgcl::compress::zstd::compress

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

Compresses the whole of the data at once into one zstd frame: the header with the content's size and a window no
larger than the data needs, blocks of up to 128 KB — each compressed, or, where that would not make it smaller,
stored as it is, or a single byte repeated given as that byte — and the XXH64 of the content. Without a dictionary
the blocks are compressed straight from the data, each block's history the data before it. Empty data is a frame of
one empty block.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.
- (1), (3), (5) The default options: level 3, the content's checksum and size, no dictionary.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the level, the checksum, the content's size, the window, a dictionary ([options](../zstd-options.md)) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data, by a factor the level sets.

## Exceptions

- (1), (3), (5) None.
- (2), (4), (6) `std::invalid_argument` when `window_log` is not 0 and outside 10 to 31.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    println("{}", compress::zstd::compress(string()).size());
    println("{}", compress::zstd::compress(string("x").repeat(1000)).size());
    println("{}", compress::zstd::compress(string("x").repeat(1000), {.checksum = false}).size());
    try {
        compress::zstd::compress("x", {.window_log = 9});
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
13
15
11
compress::zstd: a window_log of 10..31
```

## See also

- [decompress](decompress.md): the other way
- [zstd::writer](../zstd-writer/README.md): a stream
- [sgcl::compress::zstd](README.md)
