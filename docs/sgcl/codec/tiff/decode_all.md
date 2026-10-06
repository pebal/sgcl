[sgcl](../../README.md) › [codec](../README.md) › [tiff](README.md)

# sgcl::codec::tiff::decode_all

```cpp
static expected<vector<image>, error> decode_all(const slice<const byte>& data,                          // (1)
                                                 const decode_options& o = {}) noexcept;
static expected<vector<image>, error> decode_all(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes every page of a TIFF file, in the order of its chain of directories, each as [decode](decode.md) decodes
the first. The pages of one file may differ in size and format, and each comes in its own.

1. Reads the file in memory, in place.
2. Reads the stream to its end, then the file as (1).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The images, or the error of the first page that does not decode, or of the header, as [decode](decode.md) lists
them.

## Complexity

Linear in the size of the file and in the pixels of the pages.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

`o.limits.max_pixels` holds for each page, not for all of them together. A chain of more than 65536 directories
is `errc::corrupt`.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<codec::image> pages;
    pages.push_back(codec::image(100, 50, codec::pixel_format::rgb8));
    pages.push_back(codec::image(20, 20, codec::pixel_format::gray16));
    vector<codec::image> back = codec::tiff::decode_all(codec::tiff::encode(pages));
    for (const codec::image& page : back) {
        println("{}x{}, {} bytes a row", page.width(), page.height(), page.stride());
    }
}
```

Output:

```text
100x50, 300 bytes a row
20x20, 40 bytes a row
```

## See also

- [decode](decode.md): the first page
- [encode](encode.md): a TIFF of one image or several
- [sgcl::codec::tiff](README.md)
