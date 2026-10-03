[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib.md)

# sgcl::compress::zlib::compress

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

Compresses the whole of the data at once into a zlib stream: the two-byte header, the DEFLATE data and the Adler-32 of
the data, at the [level](../level.md) of the options (6 unless told otherwise). With a dictionary in the options, the
header names it by its Adler-32 (FDICT), and the reader must be given the same bytes. It never fails: any bytes
compress.

- (1–2) The bytes of `data`.
- (3–4) The bytes of the text.
- (5–6) Take part only for a literal, a character array or a `std::string_view`: the text's bytes, as (3–4) take
  them.

The encoder's tables and the room the output is gathered in are lent by the thread, kept from one call to the next,
so a small compress allocates its result and not, again, its tables.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress |
| `text` | the text to compress, its bytes as they are (UTF-8) |
| `o` | the level and a preset dictionary ([options](../zlib-options.md)) |

## Return value

The compressed bytes, a new vector.

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
    string text = string("the same words, the same words again and again. ").repeat(100);
    println("{}", compress::zlib::compress(text).size());
    println("{}", compress::zlib::compress(text, {.level = compress::level::fastest}).size());
    println("{}", compress::zlib::compress("").size());
}
```

Output:

```text
71
111
8
```

## See also

- [decompress](decompress.md): the other way
- [zlib::writer](../zlib-writer.md): a stream
- [sgcl::compress::zlib](../zlib.md)
