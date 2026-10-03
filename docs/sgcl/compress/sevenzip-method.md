[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::method

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    enum class method : uint8_t {
        lzma2,
        lzma,
        ppmd,
        deflate,
        copy
    };
}
```

The coder of a written archive's folders, the [options](sevenzip-options.md)' `method`. BZip2 and Deflate64 are read,
not written: the module has no compressor of BZip2, and the enumeration has no value for either. The
[level](level/README.md) is read by the method: LZMA and LZMA2 take xz's levels (the dictionary 256 KiB at 0 to 64 MiB at
9, the optimal parser from 4), with the dictionary written in the header no larger than the folder; PPMd takes
7-Zip's order and memory for the level (order 3 to 32, 512 KiB to 192 MiB); Deflate the level as [flate](flate/README.md)
takes it.

| Value | Description |
|---|---|
| `lzma2` | LZMA2, 7-Zip's default ([xz](xz/README.md)'s coder) |
| `lzma` | LZMA ([lzma](lzma/README.md)) |
| `ppmd` | PPMd var. H, for text |
| `deflate` | DEFLATE ([flate](flate/README.md)) |
| `copy` | the data as it is; no filter goes before it |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string text = string("the same words again and again. ").repeat(1000);
    compress::sevenzip::entry_info info;  // a fixed time: the same bytes every run
    info.modified = time::datetime::from_unix(1790000000, time::zone::utc());
    using m = compress::sevenzip::method;
    for (auto method : {m::lzma2, m::lzma, m::ppmd, m::deflate, m::copy}) {
        io::buffer archive;
        compress::sevenzip::writer w(archive, {.method = method});
        w.add("text.txt", text, info);
        (void)w.close();
        auto a = compress::sevenzip::archive::from(archive.data());
        println("{} bytes, read back {}", archive.size(), a->read("text.txt")->size());
    }
}
```

Output:

```text
240 bytes, read back 32000
238 bytes, read back 32000
194 bytes, read back 32000
261 bytes, read back 32000
32138 bytes, read back 32000
```

## See also

- [options](sevenzip-options.md)
- [sgcl::compress::sevenzip](sevenzip.md)
