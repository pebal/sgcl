[sgcl](../README.md) › [compress](README.md) › [zlib](zlib.md)

# sgcl::compress::zlib::options

```cpp
#include "sgcl/compress/zlib.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zlib {
    public:
        struct options {
            compress::level level;
            slice<const byte> dictionary;
        };
    };
}
```

`sgcl::compress::zlib::options` is how a zlib stream is made and read: the [level](level.md) of the encoder and a
preset dictionary, data both sides agree on in advance, which the first matches may refer to.
[compress](zlib/compress.md) and the [writer](zlib-writer.md) take both, and name the dictionary in the header by
its Adler-32; [decompress](zlib/decompress.md) and the [reader](zlib-reader.md) take the dictionary and do not read
the level.

## Rules

- An aggregate: `{.level = 9}`, `{.dictionary = sample}`, the other field at its default.
- The dictionary is a view of the caller's bytes: a function reads it during the call, and a writer or a reader
  copies it in its constructor, so the bytes need live no longer.
- Of a dictionary longer than the window, its last 32 KB are the history; at level 0 it is not used, though the
  header still names it.
- A stream that names a dictionary is read only with that one: without it, or with another, it is
  `errc::dictionary_required`. A dictionary given for a stream that names none is not used.
  [dictionary_id](zlib/dictionary_id.md) reads the name, so that a program with several dictionaries picks the
  right one.

## Member objects

| Member | Description |
|---|---|
| `level` | how hard the encoder works, 0 to 9 or `level::huffman_only`; 6 by default ([level](level.md)) |
| `dictionary` | the preset dictionary; empty by default: none |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string sample = "{\"name\": \"\", \"email\": \"\", \"active\": true}";
    string record = "{\"name\": \"Ann\", \"email\": \"ann@example.com\", \"active\": true}";
    compress::zlib::options o{.dictionary = slice<const byte>(sample)};

    auto packed = compress::zlib::compress(record, o);
    println("{} bytes into {}", record.size(), packed.size());
    println("{}", compress::zlib::decompress(packed).error().message());
    println("{}", compress::zlib::decompress(packed, o).has_value());
}
```

Output:

```text
59 bytes into 36
offset 6: zlib: the stream needs a preset dictionary
true
```

## See also

- [dictionary_id](zlib/dictionary_id.md): which dictionary a stream needs
- [level](level.md)
- [sgcl::compress::zlib](zlib.md)
