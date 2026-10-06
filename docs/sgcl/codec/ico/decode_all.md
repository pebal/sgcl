[sgcl](../../README.md) › [codec](../README.md) › [ico](README.md)

# sgcl::codec::ico::decode_all

```cpp
static expected<vector<image>, error> decode_all(const slice<const byte>& data,                          // (1)
                                                 const decode_options& o = {}) noexcept;
static expected<vector<image>, error> decode_all(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes every entry of an ICO or CUR file, in the order of its directory, each as [decode](decode.md) decodes the
one it chooses.

1. Reads the file in memory, in place.
2. Reads the stream to its end, then the file as (1).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The images, or the error of the first entry that does not decode, or of the directory, as
[decode](decode.md) lists them.

## Complexity

Linear in the size of the file and in the pixels of the images.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

`o.limits.max_pixels` holds for each entry, not for all of them together.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<codec::image> sizes;
    for (int side : {16, 32, 256}) {
        sizes.push_back(codec::image(side, side, codec::pixel_format::rgba8));
    }
    vector<codec::image> entries = codec::ico::decode_all(codec::ico::encode(sizes));
    for (const codec::image& entry : entries) {
        println("{}x{}", entry.width(), entry.height());
    }
}
```

Output:

```text
16x16
32x32
256x256
```

## See also

- [decode](decode.md): the largest entry
- [encode](encode.md): an ICO or CUR of one image or several
- [sgcl::codec::ico](README.md)
