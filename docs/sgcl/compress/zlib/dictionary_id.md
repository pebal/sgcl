[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib.md)

# sgcl::compress::zlib::dictionary_id

```cpp
static optional<uint32_t> dictionary_id(const slice<const byte>& data) noexcept;
```

Reads the name of the preset dictionary a zlib stream in memory was made with: the Adler-32 of the dictionary, from
the stream's header. A program with several dictionaries picks the right one by it before it calls
[decompress](decompress.md). Only the header is read.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the zlib stream, or at least its first six bytes |

## Return value

The Adler-32 of the dictionary; `nullopt` for a stream made without one, and for data that is not zlib.

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
    auto with = compress::zlib::compress("hello", {.dictionary = slice<const byte>(sample)});
    auto without = compress::zlib::compress("hello");

    auto id = compress::zlib::dictionary_id(with);
    println("{}", id == hash::adler32::of(slice<const byte>(sample)));
    println("{}", compress::zlib::dictionary_id(without).has_value());
}
```

Output:

```text
true
false
```

## See also

- [reader::dictionary_id](../zlib-reader/dictionary_id.md): the same, of a stream
- [options](../zlib-options.md)
- [sgcl::compress::zlib](../zlib.md)
