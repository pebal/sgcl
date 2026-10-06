[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::image

```cpp
#include "sgcl/codec/image.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class image;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::codec::image` is an image in memory: its size, its [pixel format](../pixel_format.md) and its pixels, row after
row with no padding, plus the metadata of the file it came from, the EXIF block and the ICC profile as bytes and
EXIF's orientation, which a program may set too. Every decoder of the module returns one and every encoder takes
one. Where Go's `image.Image` is an interface with a type per layout of a pixel, an `image` is one class with the
layout as a value; where a `std::vector<uint8_t>` with two sides would be copied, an `image` is shared.

It is a handle of one word: a `tracked_ptr` to its state, which holds the sizes, the format, the metadata and the
pixels in one managed block of the exact size. A copy shares the pixels, so a write through one copy is seen through
the other; [clone](clone.md) makes new ones. A slice of the pixels, from [pixels](pixels.md) or
[row](row.md), keeps the block alive after the image is gone, as a Go slice keeps its array. There is no
empty image: the constructor takes the sides and the format, and a decoder makes the image of a file. A move is a copy
of the word, as the move of a [tracked_ptr](../../core/tracked_ptr/README.md) is: a moved-from image is still the image, and
its members and every function that takes an image work on it as on the image it was moved into.

A decoder returns an [expected](../../core/expected/README.md)`<image, codec::error>`: the image, or why the file is not one. A
program that takes the image as it comes writes `codec::image photo = codec::png::decode(file);`, and a file that is
not an image throws a [bad_expected_access](../../core/bad_expected_access/README.md)`<codec::error>` that carries the error.

## Rules

- The pixels and the metadata are not synchronized: concurrent readers, or one writer, with the program's own
  synchronization, and that counts every copy of the handle, since copies share them. [convert](convert.md),
  [clone](clone.md), [oriented](oriented.md), [cropped](cropped.md), [flipped](flipped.md), [rotated](rotated.md) and
  the encoders only read.
- The decoders never turn an image: the rows are as the file stores them, and [orientation](orientation.md)
  says how they are meant to be shown, as with Go and libjpeg. [oriented](oriented.md) turns them.
- [save](save.md) is declared with the class and defined in `sgcl/codec/files.h`, with
  [codec::save](../save.md), which `sgcl/codec.h` brings.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](image.md) | an image of all-zero pixels |

#### Size and format

| Function | Description |
|---|---|
| [width](width.md) | the width in pixels |
| [height](height.md) | the height in pixels |
| [format](format.md) | the pixel format |
| [stride](stride.md) | the bytes of a row |

#### Pixels

| Function | Description |
|---|---|
| [pixels](pixels.md) | every row, from the top |
| [row](row.md) | one row |

#### New images

| Function | Description |
|---|---|
| [convert](convert.md) | the same pixels in another format |
| [clone](clone.md) | a copy of the pixels and the metadata |
| [oriented](oriented.md) | the image turned and mirrored as it is meant to be shown |
| [cropped](cropped.md) | a rectangle of the pixels |
| [flipped](flipped.md) | the image mirrored, left to right or top to bottom |
| [rotated](rotated.md) | the image turned by a multiple of 90 degrees |

#### Metadata

| Function | Description |
|---|---|
| [exif](exif.md) | the file's EXIF block |
| [icc](icc.md) | the file's ICC profile |
| [orientation](orientation.md) | EXIF's orientation, 1 to 8 |
| [set_exif](set_exif.md) | sets the EXIF block |
| [set_icc](set_icc.md) | sets the ICC profile |
| [set_orientation](set_orientation.md) | sets the orientation, and the tag of the EXIF block with it |

#### Files

| Function | Description |
|---|---|
| [save, async_save](save.md) | writes the image to a file in the format its extension names |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a row of red, green and blue, and a row of white
    codec::image picture(3, 2, codec::pixel_format::rgb8);
    const int values[] = {255, 0, 0, 0, 255, 0, 0, 0, 255,
                          255, 255, 255, 255, 255, 255, 255, 255, 255};
    for (int i : range(18)) {
        picture.pixels()[i] = byte(values[i]);
    }
    codec::image luma = picture.convert(codec::pixel_format::gray8);
    println("{}x{}, {} bytes a row", luma.width(), luma.height(), luma.stride());
    for (byte v : luma.row(0)) {
        println(std::to_integer<int>(v));
    }
    codec::image wide = picture.convert(codec::pixel_format::rgb16);
    println("{} bytes a row at 16 bits", wide.stride());
}
```

Output:

```text
3x2, 3 bytes a row
76
150
29
18 bytes a row at 16 bits
```

## See also

- [pixel_format](../pixel_format.md): the layouts of a pixel
- [load](../load.md), [decode](../decode.md): an image of a file, of any format the module reads
- [png](../png/README.md), [jpeg](../jpeg/README.md), [gif](../gif/README.md), [webp](../webp/README.md), [heif](../heif/README.md): each format
- [save](../save.md): an image into a file
- [sgcl::codec](../README.md)
