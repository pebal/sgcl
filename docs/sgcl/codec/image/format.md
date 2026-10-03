[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::format

```cpp
pixel_format format() const noexcept;
```

The [pixel format](../pixel_format.md) of the image: how its pixels lie in its rows. A decoder makes the file's own
format unless [decode_options](../decode_options.md)`.want` asks for another: a gray PNG stays `gray8` or `gray16`,
a CMYK JPEG is `cmyk8`. The format of an image does not change; [convert](convert.md) makes a new image in another.

## Parameters

None.

## Return value

The pixel format.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image scan(8, 8, codec::pixel_format::gray8);
    vector<byte> file = codec::png::encode(scan);
    codec::image own = codec::png::decode(file);
    codec::image color = codec::png::decode(file, {.want = codec::pixel_format::rgba8});
    println("{}", own.format() == codec::pixel_format::gray8);
    println("{}", color.format() == codec::pixel_format::rgba8);
}
```

Output:

```text
true
true
```

## See also

- [pixel_format](../pixel_format.md): the formats
- [convert](convert.md): the pixels in another format
- [sgcl::codec::image](README.md)
