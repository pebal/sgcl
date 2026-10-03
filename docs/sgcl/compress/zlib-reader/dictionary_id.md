[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib.md) › [reader](../zlib-reader.md)

# sgcl::compress::zlib::reader::dictionary_id

```cpp
optional<uint32_t> dictionary_id() const noexcept;
```

Returns the name of the preset dictionary the stream asks for, the Adler-32 in its header, once the reader has read
the header. A read of a stream that names a dictionary the reader was not given fails with
`errc::dictionary_required`, and this says which one to give a new reader.

## Parameters

None.

## Return value

The Adler-32 of the dictionary; `nullopt` before the header is read, and for a stream that names none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string sample = "hello world";
    auto packed = compress::zlib::compress("hello", {.dictionary = slice<const byte>(sample)});

    compress::zlib::reader r{io::buffer(packed)};
    println("{}", r.dictionary_id().has_value());
    auto text = r.read_all_text();
    println("{}", r.last_error()->message());
    println("{}", r.dictionary_id() == hash::adler32::of(slice<const byte>(sample)));
}
```

Output:

```text
false
offset 6: zlib: the stream needs a preset dictionary
true
```

## See also

- [zlib::dictionary_id](../zlib/dictionary_id.md): the same, of data in memory
- [sgcl::compress::zlib::reader](../zlib-reader.md)
