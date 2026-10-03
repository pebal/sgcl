[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::limits

```cpp
#include "sgcl/codec/options.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    struct limits {
        uint64_t max_pixels = 100'000'000;
        size_t max_metadata = 64u << 20;
    };
}
```

`sgcl::codec::limits` is what a decoder accepts before it allocates: the pixels a file may claim and the bytes of
metadata it may carry. A small file can claim a huge image, a pixel bomb: every decoder checks the size a file
declares against `max_pixels` before it allocates anything, and the EXIF and ICC it reads against `max_metadata` as
it reads them. Past either, the decoding is `errc::too_large`, with nothing allocated for the image. A file that
claims a side of zero is `errc::corrupt`. Pillow warns from about 89 million pixels and refuses from about 179
million; Go has no limit.

The limits are the field `limits` of [decode_options](decode_options.md), and every decoder of the module takes
them: [decode](decode.md), [load](load.md), [decode_frames](decode_frames.md) and each format's own `decode`. A
plain struct, filled by designated initializers: `{.limits = {.max_pixels = 20'000'000}}`.

## Member objects

| Member | Description |
|---|---|
| `max_pixels` | the most pixels a file may claim, its width times its height (an animation's canvas); 100 million by default. Past about 2^60 pixels, more than an image of 8 bytes a pixel can address, a file is `errc::too_large` whatever this says |
| `max_metadata` | the most bytes of one block of EXIF or of the ICC profile; 64 MB by default |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(2000, 1000, codec::pixel_format::gray8);
    vector<byte> png = codec::png::encode(picture);
    println("{}", codec::decode(png).has_value());
    println(codec::decode(png, {.limits = {.max_pixels = 1'000'000}}).error().message());

    vector<byte> jpeg = io::read_file("tests/codec/fuzz/seeds/jpeg_decode/exif_9x7.jpg");
    println(codec::decode(jpeg, {.limits = {.max_metadata = 16}}).error().message());
}
```

Output:

```text
true
offset 8: size limit exceeded
offset 2: jpeg: EXIF past limits.max_metadata
```

## See also

- [decode_options](decode_options.md): what holds them
- [errc](errc.md): `too_large`
- [codec](README.md)
