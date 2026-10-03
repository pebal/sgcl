[sgcl](../README.md) › [codec](README.md) › [png](png/README.md)

# sgcl::codec::png::options

```cpp
#include "sgcl/codec/png.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class png {
    public:
        struct options {
            compress::level level = 7;
        };
    };
}
```

`sgcl::codec::png::options` is what [encode](png/encode.md) is told: the DEFLATE level of the zlib stream the rows
are written through, a plain struct, written in place as `{.level = 9}`. The level is compress's
[level](../compress/level/README.md): 0 stores, 1 is the fastest, 9 the smallest, and `level::huffman_only` codes
the bytes with no search for repeats. An `int` converts to it, and one outside 0 to 9 is `invalid_argument` when the
options are made (at compile time in a constant).

The default here is 7, not compress's 6: the first of DEFLATE's chain levels, which the filtered strategy of
PNG's rows is for ([encode](png/encode.md)). At level 0 the rows are stored unfiltered.

## Member objects

| Member | Description |
|---|---|
| `level` | the DEFLATE level of the image data, 0 to 9 or `compress::level::huffman_only`; 7 unless told |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(256, 256, codec::pixel_format::rgb8);
    for (int y : range(256)) {
        slice<byte> row = picture.row(y);
        for (int i : range(3 * 256)) {
            row[i] = byte((i * y) >> 8);
        }
    }
    for (int level : {0, 1, 6, 7, 9}) {
        vector<byte> file = codec::png::encode(picture, {.level = level});
        println("level {}: {} bytes", level, file.size());
    }
}
```

Output:

```text
level 0: 196983 bytes
level 1: 19895 bytes
level 6: 13306 bytes
level 7: 9659 bytes
level 9: 9421 bytes
```

## See also

- [encode](png/encode.md): what takes the options
- [save_options](save_options.md): the level `save` writes a PNG at
- [compress::zlib](../compress/zlib/README.md): the stream of the image data
- [sgcl::codec::png](png/README.md)
