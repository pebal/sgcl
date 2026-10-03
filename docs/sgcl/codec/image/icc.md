[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::icc

```cpp
slice<const byte> icc() const noexcept;
```

The ICC profile of the file the image was decoded from, as the file had it: PNG's `iCCP` decompressed, JPEG's APP2
segments joined, WebP's `ICCP`, the profile the system gives for HEIF. GIF has none. It is empty when the file had
none, when [decode_options](../decode_options.md)`.metadata` was false and for an image the program made, until
[set_icc](set_icc.md) sets one.

The decoders of PNG, JPEG and WebP do not apply the profile: the pixels are the values the file stores, and the
profile says what colors they mean. [convert](convert.md), [clone](clone.md) and [oriented](oriented.md) carry it
to the new image as it is, and [png::encode](../png/encode.md), [jpeg::encode](../jpeg/encode.md) and
[heif::encode](../heif/encode.md) write it back into the file, JPEG's when it fits 255 segments of 65 519 bytes.

## Parameters

None.

## Return value

The bytes of the profile, or an empty slice.

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
    codec::image made(9, 7, codec::pixel_format::rgb8);
    println("{} bytes", made.icc().size());
    // the bytes of a profile, which the module does not read
    made.set_icc(vector<byte>(300, byte('p')));
    vector<byte> file = codec::jpeg::encode(made);
    println("{} bytes through JPEG", codec::jpeg::decode(file)->icc().size());
    println("{} bytes", codec::jpeg::decode(file, {.metadata = false})->icc().size());
}
```

Output:

```text
0 bytes
300 bytes through JPEG
0 bytes
```

## See also

- [exif](exif.md): the EXIF block
- [set_icc](set_icc.md): sets the ICC profile
- [sgcl::codec::image](README.md)
