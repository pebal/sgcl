[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib.md) › [reader](../zlib-reader.md)

# sgcl::compress::zlib::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;             // (1)
reader(const io::reader& in, const options& o) noexcept;    // (2)
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header.

1. A reader with no dictionary.
2. A reader with the dictionary of the options (its level is not read): the bytes are copied, and of more than 32 KB the
   last 32 KB are the history; it is used when the stream names it by its Adler-32, and a stream that names another is
   `errc::dictionary_required`.

The decoder's memory (its window and state) is taken here, and kept across a [reset](reset.md).

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `o` | the dictionary the stream was made with ([options](../zlib-options.md)) |

## Complexity

Constant; the decoder's window is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string sample = "{\"user\": \"\", \"action\": \"\", \"time\": \"\"}";
    compress::zlib::options o{.dictionary = slice<const byte>(sample)};
    string message = "{\"user\": \"ann\", \"action\": \"login\", \"time\": \"09:30\"}";
    auto packed = compress::zlib::compress(message, o);
    println("{} bytes into {}", message.size(), packed.size());

    compress::zlib::reader r{io::buffer(packed), o};
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
51 bytes into 33
{"user": "ann", "action": "login", "time": "09:30"}
```

## See also

- [read](read.md)
- [sgcl::compress::zlib::reader](../zlib-reader.md)
