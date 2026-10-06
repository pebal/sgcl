[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::from_exif

```cpp
static metadata from_exif(const slice<const byte>& exif, const string& xmp = {}) noexcept;
```

The metadata of an EXIF block and an XMP packet in hand: the block as [image::exif](../image/exif.md) keeps it (a
TIFF structure from its byte-order mark), and the packet as text. The fields are read as [read](read.md) reads a
file's, EXIF first; nothing limits the sizes, since the blocks are already in memory.

## Parameters

| Parameter | Description |
|---|---|
| `exif` | the EXIF block, a TIFF structure; empty for none |
| `xmp` | the XMP packet; empty for none |

## Return value

The metadata; of nothing when neither block reads.

## Complexity

Linear in the sizes of the blocks.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image photo = codec::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    codec::metadata m = codec::metadata::from_exif(photo.exif());
    println("{}, f/{}", m.model().value_or("none"), m.f_number().value_or(0));
    println("{}", codec::metadata::from_exif({}, "<x/>").make().has_value());
}
```

Output:

```text
Canon EOS R5, f/2.8
false
```

## See also

- [image::exif](../image/exif.md): the block a decoded image keeps
- [read](read.md): of a file
- [sgcl::codec::metadata](README.md)
