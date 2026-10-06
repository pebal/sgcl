[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::brotli

```cpp
#include "sgcl/compress/brotli.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class brotli;
}
```

`sgcl::compress::brotli` is Brotli (`.br`, RFC 7932): the format of HTTP's `Content-Encoding: br` and of WOFF2 fonts,
which every browser reads. It is LZ77 with prefix codes chosen by context: each literal is coded by one of many trees,
picked by the two bytes before it, and the literals, the commands (an insert length with a copy length) and the
distances are each cut into blocks of types that switch their codes as the data changes. A copy may reach past the
window into a static dictionary of 13,504 words of the web's languages, transformed by one of 121 transforms (upper
case, a suffix, a word cut short). A stream is the window's size and meta-blocks to the last; it carries no checksum
and no size. It is a class of static functions and the types of the format: a stream in memory either way
([compress](compress.md), [decompress](decompress.md)), and a stream each way, a [writer](../brotli-writer/README.md)
and a [reader](../brotli-reader/README.md). Go's standard library has no brotli.

**Levels** are brotli's qualities ([level](../brotli-level/README.md)): 0 to 11 as `brotli -q N`, 11 the default.
Every quality makes the same format, decoded at the same speed.

## Rules

- **The defaults are the `brotli` command's**: quality 11, a window of 2^22 - 16 bytes ([options](../brotli-options.md)).
  [compress](compress.md) makes the window no larger than the data needs; a writer announces the options'.
- **One stream.** The format has no frame and no magic: data after a stream's last meta-block is `errc::corrupt` for
  [decompress](decompress.md), and left unread by a [reader](../brotli-reader/README.md).
- **Checks.** A stream has no checksum; the RFC's rules are checked: complete prefix codes, a context map within its
  trees, a distance within the data or the dictionary, meta-blocks that hold what their lengths say, padding bits of
  zero (`errc::corrupt`); the window of the large-window extension, which RFC 7932 does not have, is
  `errc::invalid_header`.
- **Memory.** A stream's window is held against the [limits](../limits.md)' `max_memory` before anything is taken;
  `decompress` has `max_size` as well.
- Options out of range (a `window_log` outside 10 to 24) are the program's mistake: `compress` throws
  `std::invalid_argument`, and a writer's first write reports `errc::invalid_argument`. A level out of range is refused
  by [level](../brotli-level/README.md) itself.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [level](../brotli-level/README.md) | how hard the compressor works: brotli's quality, 0 to 11 |
| [options](../brotli-options.md) | the quality and the window |
| [writer](../brotli-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../brotli-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed into one stream (static) |
| [decompress](decompress.md) | a stream decompressed and checked, against the limits (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words, the same words again. ").repeat(100);
    vector<byte> fast = compress::brotli::compress(text, {.level = 1});
    vector<byte> small = compress::brotli::compress(text);
    println("{} bytes into {} or {}", text.size(), fast.size(), small.size());

    auto back = compress::brotli::decompress(small);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", back->size());
}
```

Output:

```text
5400 bytes into 47 or 31
5400
```

## See also

- [brotli::writer](../brotli-writer/README.md), [brotli::reader](../brotli-reader/README.md): the streams
- [benchmarks](../benchmarks.md): against libbrotli
- `tests/compress/brotli.cpp`: libbrotli as the oracle both ways, streams written by hand from RFC 7932;
  `tests/compress/fuzz/brotli_fuzz.cpp`
- [sgcl::compress](../README.md)
