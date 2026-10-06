[sgcl](../../README.md) › [compress](../README.md) › [brotli](README.md)

# sgcl::compress::brotli::compress

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

Compresses the whole of the data at once into one brotli stream: the window's size, no larger than the data needs,
then meta-blocks of up to 1 MB of input (256 KB below quality 5), each with prefix codes of its own, or stored as it
is where that would not make it smaller, the last marked as the last. The meta-blocks are compressed straight from the
data, each one's history the data before it. Empty data is a stream of one byte.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.
- (1), (3), (5) The default options: quality 11, a window of 2^22 - 16 bytes.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the quality and the window ([options](../brotli-options.md)) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data, by a factor the quality sets.

## Exceptions

- (1), (3), (5) None.
- (2), (4), (6) `std::invalid_argument` when `window_log` is outside 10 to 24.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    println("{}", compress::brotli::compress(string()).size());
    println("{}", compress::brotli::compress(string("x").repeat(1000)).size());
    println("{}", compress::brotli::compress(string("x").repeat(1000), {.level = 0}).size());
    try {
        compress::brotli::compress("x", {.window_log = 25});
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
2
11
11
compress::brotli: a window_log of 10..24
```

## See also

- [decompress](decompress.md): the other way
- [brotli::writer](../brotli-writer/README.md): a stream
- [sgcl::compress::brotli](README.md)
