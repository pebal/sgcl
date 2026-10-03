[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md)

# sgcl::compress::gzip::compress

```cpp
/*(1)*/ static vector<byte> compress(const slice<const byte>& data) noexcept;
/*(2)*/ static vector<byte> compress(const slice<const byte>& data, const options& o);
/*(3)*/ static vector<byte> compress(const string& text) noexcept;
/*(4)*/ static vector<byte> compress(const string& text, const options& o);
/*(5)*/ template<class T>
        static vector<byte> compress(const T& text) noexcept;
/*(6)*/ template<class T>
        static vector<byte> compress(const T& text, const options& o);
```

Compresses the whole of the data at once into one gzip member: the header of the options, the DEFLATE data, the CRC-32
and the length of the data, at the [level](../level.md) of the options (6 unless told otherwise). The header of the
options is written as it is: a name and a comment converted from UTF-8 to ISO 8859-1, or past it written as their
UTF-8 bytes, as gzip(1) writes a file's name ([gzip_header](../gzip_header.md)); the time as Unix seconds (0, "not
known", for none or one before 1970 or past 2106), the level's hint (2 for level 9, 4 for level 1), the operating system
byte (255, unknown, unless set). It never fails: any bytes compress.

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
| `o` | the level and the header ([options](../gzip-options.md)) |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data, by a factor the level sets.

## Exceptions

- (1), (3), (5) None: the default header is always written.
- (2), (4), (6) `std::invalid_argument` when the header of the options cannot be written: a name or a comment with a
  NUL, an extra field past 65 535 bytes; its `what()` says which. The function returns
  no error, so a header the format cannot hold is the program's mistake.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(100);
    println("{}", compress::gzip::compress(text).size());
    println("{}", compress::gzip::compress(text, {.level = compress::level::fastest}).size());
    println("{}", compress::gzip::compress("").size());
}
```

Output:

```text
83
123
20
```

## See also

- [decompress](decompress.md): the other way
- [gzip::writer](../gzip-writer.md): a stream
- [sgcl::compress::gzip](../gzip.md)
