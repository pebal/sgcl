[sgcl](../README.md) › [compress](README.md) › [gzip](gzip/README.md)

# sgcl::compress::gzip::options

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class gzip {
    public:
        struct options {
            compress::level level;
            gzip::header header;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::gzip::options` is how a gzip stream is made: the [level](level/README.md) of the encoder and the
[header](gzip_header.md) written before the data — a name, a comment, the time the data was modified, an extra
field, the system that made it. [compress](gzip/compress.md) and the [writer](gzip-writer/README.md) take it.

## Rules

- An aggregate: `{.level = 9}`, `{.header = h}`, the other field at its default.
- The header's strings are ISO 8859-1 in the format, and UTF-8 past it, as gzip(1) writes a file's name. A NUL, or
  an extra field past 65 535 bytes, cannot be written — `compress` throws `std::invalid_argument`, the writer's first
  write gives `errc::invalid_argument` ([gzip_header](gzip_header.md)).

## Member objects

| Member | Description |
|---|---|
| `level` | how hard the encoder works, 0 to 9 or `level::huffman_only`; 6 by default ([level](level/README.md)) |
| `header` | the header of the member; empty by default: no name, no comment, no time, the system unknown (255) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::gzip::options o{.level = 9};
    o.header.name = "notes.txt";

    auto packed = compress::gzip::compress("some notes", o);
    compress::gzip::reader r{io::buffer(packed)};
    println("{}", r.header()->name);
}
```

Output:

```text
notes.txt
```

## See also

- [gzip_header](gzip_header.md)
- [level](level/README.md)
- [sgcl::compress::gzip](gzip/README.md)
