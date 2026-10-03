[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::exif

```cpp
slice<const byte> exif() const noexcept;
```

The EXIF block of the file the image was decoded from, as the file had it: a TIFF structure from its byte-order mark
(`II` or `MM`), without the `Exif` header of JPEG's segment. The decoders of PNG (`eXIf`), JPEG (APP1) and WebP
(`EXIF`) keep it; GIF has none, and HEIF's decoder leaves it empty, since the system gives the tags and not the
block. It is empty, too, when the file had none, when [decode_options](../decode_options.md)`.metadata` was false
and for an image the program made, until [set_exif](set_exif.md) sets one.

The module reads and writes one field of it, the orientation ([orientation](orientation.md)); the rest is the
program's to read. [convert](convert.md) and [clone](clone.md) carry it to the new image as it is,
[oriented](oriented.md) with its orientation tag set to 1, and [png::encode](../png/encode.md) and
[jpeg::encode](../jpeg/encode.md) write it back into the file, JPEG's when it fits one segment, 65 527 bytes.

## Parameters

None.

## Return value

The bytes of the block, or an empty slice.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

// A TIFF structure of one tag, the orientation (0x0112) o, big-endian ("MM")
vector<byte> exif_of(int o) {
    vector<byte> block;
    for (int v : {0x4D, 0x4D, 0, 42, 0, 0, 0, 8, 0, 1, 1, 0x12, 0, 3, 0, 0, 0, 1, 0, o}) {
        block.push_back(byte(v));
    }
    block.resize(26);  // the rest of the tag's value and the end of the directories: zeros
    return block;
}

int main() {
    codec::image made(9, 7, codec::pixel_format::rgb8);
    println("{} bytes", made.exif().size());
    made.set_exif(exif_of(6));
    vector<byte> file = codec::png::encode(made);
    codec::image photo = codec::png::decode(file);
    slice<const byte> exif = photo.exif();
    println("{} bytes, {}{}", exif.size(), char(exif[0]), char(exif[1]));
    println("orientation {}", photo.orientation());
    codec::image bare = codec::png::decode(file, {.metadata = false});
    println("{} bytes", bare.exif().size());
}
```

Output:

```text
0 bytes
26 bytes, MM
orientation 6
0 bytes
```

## See also

- [orientation](orientation.md): the one field the module reads
- [set_exif](set_exif.md): sets the EXIF block
- [icc](icc.md): the color profile
- [sgcl::codec::image](README.md)
