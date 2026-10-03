[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::save_options

```cpp
#include "sgcl/codec/files.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    struct save_options {
        compress::level level = 7;
        int quality = 85;
        jpeg::subsampling subsampling = jpeg::subsampling::s420;
    };
}
```

`sgcl::codec::save_options` is what [save](save.md), [async_save](save.md) and [image::save](image/save.md) write
with, for whichever format the path names: a plain struct, filled by designated initializers in the order of its
fields, `picture.save("small.jpg", {.quality = 70, .subsampling = codec::jpeg::subsampling::s444})`. Each field is
for the formats it names, and the others leave it alone: `{.level = 1}` changes nothing of a JPEG. The options of each
format's own `encode` ([png::options](png-options.md), [jpeg::options](jpeg-options.md),
[heif::options](heif-options.md)) are where the rest is: `jpeg::options::optimize` has no field here, and `save`
writes a JPEG with the standard Huffman tables.

## Member objects

| Member | Description |
|---|---|
| `level` | PNG: the level of the zlib stream, 0 stores, 1 fastest, 9 smallest; 7 by default, the default of [png::options](png-options.md), a constant: the default options are made without a throw. A number [compress::level](../compress/README.md) does not take throws `invalid_argument` where the options are made |
| `quality` | JPEG and HEIC: 1 to 100, the IJG's scale for JPEG; 85 by default. Outside 1 to 100, `save` of a JPEG throws `invalid_argument` and `save` of a HEIC gives `errc::invalid_argument` |
| `subsampling` | JPEG: the resolution of the chrominance, [jpeg::subsampling](jpeg-subsampling.md); `s420` (4:2:0) by default |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(320, 200, codec::pixel_format::rgb8);
    picture.save("small.jpg", {.quality = 70, .subsampling = codec::jpeg::subsampling::s444});
    picture.save("fast.png", {.level = 1});

    for (const char* path : {"small.jpg", "fast.png"}) {
        codec::image loaded = codec::load(path);
        println("{}: {}x{}", path, loaded.width(), loaded.height());
        io::remove(path);
    }
}
```

Output:

```text
small.jpg: 320x200
fast.png: 320x200
```

## See also

- [save](save.md): what takes them
- [png::options](png-options.md), [jpeg::options](jpeg-options.md), [heif::options](heif-options.md): the options of
  each format's `encode`
- [codec](README.md)
