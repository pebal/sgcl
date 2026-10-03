[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::decode

```cpp
#include "sgcl/codec/decode.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                  const decode_options& o = {}) noexcept;
    expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
}
```

Decodes a file of any of the module's formats, told by its signature ([sniff](sniff.md)): [png](png.md),
[jpeg](jpeg.md), [webp](webp.md), the first frame of a [gif](gif.md), and HEIF and AVIF through the system's codec
([heif](heif.md)). Once the format is known it is that format's own `decode`, with the same options; a file of no
format the module reads is `errc::unsupported`. It is Go's `image.Decode` with the decoders registered, and what
[load](load.md) does with the bytes of a file.

1. The file in memory, read in place.
2. The file from a stream: the first bytes are read to tell the format, and the stream is handed on to the format's
   decoder with them, so nothing is read twice.

[decode_options](decode_options.md) set what differs from the defaults: the pixel format wanted, the
[limits](limits.md) a file may not pass, and whether EXIF and ICC are read. An error is a value: a program that takes
the image as it is, `codec::image photo = codec::decode(file);`, gets a
[bad_expected_access](../core/bad_expected_access.md) thrown in place of an image when the file does not decode.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits, and whether the metadata is read |

## Return value

The image, in the pixel format `o.want` names, or in the file's own when it names none; or the [error](error.md):
`errc::unsupported` for a file of no format the module reads (and for HEIF and AVIF where the system has no codec),
`errc::too_large` past `o.limits`, `errc::invalid_argument` for an `o.want` outside the list of pixel formats, the
decoder's error for data the format does not allow, and (2) `errc::io` when the stream fails, with io's error inside.

## Complexity

Linear in the size of the file and of the image.

## Exceptions

- (1) None.
- (2) What the read of the stream throws: an `io::reader` calls the read of the object it holds.

## Notes

A stream is read as it comes, so that the memory a decoding takes is the image and a constant, not the file. Two
formats keep more: a progressive JPEG keeps its coefficients whole until its last scan, and HEIF and AVIF are read to
the end of the stream before the system's codec sees them.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(320, 200, codec::pixel_format::rgb8);
    vector<byte> png = codec::png::encode(picture);
    vector<byte> jpeg = codec::jpeg::encode(picture, {.quality = 90});

    codec::image photo = codec::decode(jpeg);
    println("{}x{}, rgb8: {}", photo.width(), photo.height(),
            photo.format() == codec::pixel_format::rgb8);
    codec::image rgba = codec::decode(png, {.want = codec::pixel_format::rgba8});
    println("rgba8: {}", rgba.format() == codec::pixel_format::rgba8);

    vector<byte> gif = io::read_file("tests/codec/fuzz/seeds/gif_decode/welcome2.gif");
    codec::image first = codec::decode(gif);
    println("the first frame of a GIF: {}x{}", first.width(), first.height());

    io::buffer stream;
    stream.write(png);
    expected<codec::image, codec::error> streamed = codec::decode(stream);
    println("from a stream: {}x{}", streamed->width(), streamed->height());

    vector<byte> zeros(16);
    println(codec::decode(zeros).error().message());
}
```

Output:

```text
320x200, rgb8: true
rgba8: true
the first frame of a GIF: 290x48
from a stream: 320x200
offset 0: not an image format the module reads
```

## See also

- [load](load.md): the same of a file on disk
- [decode_frames](decode_frames.md): every frame of an animation
- [sniff](sniff.md): the format of a file's first bytes
- [decode_options](decode_options.md), [image](image.md), [error](error.md)
- [codec](README.md)
