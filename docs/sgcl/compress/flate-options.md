[sgcl](../README.md) › [compress](README.md) › [flate](flate.md)

# sgcl::compress::flate::options

```cpp
#include "sgcl/compress/flate.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class flate {
    public:
        struct options {
            compress::level level;
            slice<const byte> dictionary;
        };
    };
}
```

`sgcl::compress::flate::options` is how DEFLATE data is made and read: the [level](level.md) of the encoder and a
preset dictionary, data both sides agree on in advance, which the first matches may refer to (Go's
`NewWriterDict` and `NewReaderDict`). [compress](flate/compress.md) and the [writer](flate-writer.md) take both;
[decompress](flate/decompress.md) and the [reader](flate-reader.md) take the dictionary and do not read the level.

## Rules

- An aggregate: `{.level = 9}`, `{.dictionary = sample}`, the other field at its default.
- The dictionary is a view of the caller's bytes: a function reads it during the call, and a writer or a reader
  copies it in its constructor, so the bytes need live no longer.
- Of a dictionary longer than the window, its last 32 KB are the history; put what is most alike the data at its
  end. At level 0 the dictionary is not used. The reader must be given the same bytes: DEFLATE does not name the
  dictionary, and other bytes decode to other data or to `errc::corrupt` ([zlib](zlib.md) names it).

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
    string sample = "GET / HTTP/1.1\r\nHost: \r\nUser-Agent: \r\nAccept: */*\r\n\r\n";
    string request = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n"
                     "User-Agent: curl\r\nAccept: */*\r\n\r\n";

    compress::flate::options o{.dictionary = slice<const byte>(sample)};
    println("without: {}", compress::flate::compress(request).size());
    println("with:    {}", compress::flate::compress(request, o).size());

    auto back = compress::flate::decompress(compress::flate::compress(request, o), o);
    println("{}", string(slice<const byte>(*back)) == request);
}
```

Output:

```text
without: 80
with:    35
true
```

## See also

- [level](level.md)
- [zlib::options](zlib-options.md): the dictionary named in the header
- [sgcl::compress::flate](flate.md)
