# sgcl::codec::image

```cpp
#include "sgcl/codec/image.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    enum class pixel_format : uint8_t {
        gray8, gray_alpha8, rgb8, rgba8,           // 8 bits a channel
        gray16, gray_alpha16, rgb16, rgba16,       // 16 bits, in the byte order of the machine
        cmyk8                                      // the ink of each channel, as a CMYK JPEG holds it
    };

    class image {
    public:
        image(uint32_t width, uint32_t height, pixel_format f);   // all zero; a side of 0: invalid_argument

        uint32_t width() const noexcept;
        uint32_t height() const noexcept;
        pixel_format format() const noexcept;
        size_t stride() const noexcept;                 // the bytes of a row: width × the bytes of a pixel

        slice<byte> pixels();                           // every row, from the top
        slice<const byte> pixels() const;
        slice<byte> row(uint32_t y);                    // y past the last row: out_of_range
        slice<const byte> row(uint32_t y) const;

        image convert(pixel_format f) const;            // a new image of the same pixels in format f
        image clone() const;                            // a new image of the same format

        slice<const byte> exif() const noexcept;        // the file's EXIF (a TIFF structure) and ICC profile,
        slice<const byte> icc() const noexcept;         // empty when it had none
        uint8_t orientation() const noexcept;           // EXIF's 1..8; 1 when the file said nothing
        image oriented() const;                         // turned and mirrored as orientation() says

        expected<void, error> save(const string& path, const save_options& o = {}) const;   // the format by the extension
        async::task<expected<void, error>> async_save(const string& path, const save_options& o = {}) const;
    };
}
```

An image: its size, its pixel format and its pixels, row after row with no padding, plus the metadata the file had.

**A handle of one word.** Copies share the pixels; `clone()` makes new ones. The pixels live in one managed block. A slice of them, from `pixels()` or `row()`, keeps that block alive after the image is gone.

**Pixel formats.** Alpha is straight (not premultiplied), as PNG, GIF and WebP store it. 16-bit channels are in the byte order of the machine: the decoders turn PNG's big-endian samples around, and the encoders turn them back. `cmyk8` is the ink of each channel, 0 for none and 255 for full, as Go's `image.CMYK` has it.

**convert** always makes a copy, even to the same format:

- Between depths a channel goes up as `v × 257` (255 becomes 65535) and down to the nearest value, `round(v / 257)`. This is libpng's `png_set_scale_16`. Go and libpng's `strip_16` truncate (`v >> 8`), so an image taken down to 8 bits by the module can differ from Go's by one level.
- Color to gray is the luma of Rec. 601 (0.299, 0.587, 0.114 in 16-bit fixed point). A gray pixel keeps its value through color and back.
- A format without alpha drops it; one with alpha gets it opaque where the source had none.
- CMYK and RGB convert through each other with no color profile: each channel is `(1 − c)(1 − k)`.

**save** writes the image to a file in the format the path's extension names (`.png`, `.jpg`/`.jpeg`, `.heic` on macOS), through `path + ".part"` and a rename. It is `codec::save(image, path, options)`, and the [module's page](README.md#files) has the rules. `save` comes with `codec.h`, which also brings the encoders; `image.h` alone declares it.

**oriented** returns the image as it is meant to be shown: mirrored and turned by EXIF's orientation, with width and height swapped for 5 to 8, and `orientation()` 1. The decoders never turn an image themselves (neither do Go and libjpeg). The EXIF bytes stay as they were.

## Members

### pixel_format

```cpp
enum class pixel_format : uint8_t {
    gray8, gray_alpha8, rgb8, rgba8, gray16, gray_alpha16, rgb16, rgba16, cmyk8
};
```

The layouts of a pixel: 8 or 16 bits a channel, alpha straight, 16-bit channels in the byte order of the machine, and CMYK's ink.

### image

```cpp
image(uint32_t width, uint32_t height, pixel_format f);
```

A new image of all-zero pixels; a side of 0 is `invalid_argument`.

### width, height, format, stride

```cpp
uint32_t width() const noexcept;
uint32_t height() const noexcept;
pixel_format format() const noexcept;
size_t stride() const noexcept;
```

The size, the pixel format, and the bytes of a row: the width times the bytes of a pixel, with no padding.

### pixels, row

```cpp
slice<byte> pixels();
slice<const byte> pixels() const;
slice<byte> row(uint32_t y);
slice<const byte> row(uint32_t y) const;
```

Every row from the top, or one row; `y` past the last row is `out_of_range`. A slice keeps the pixels alive after the image is gone.

### convert, clone

```cpp
image convert(pixel_format f) const;
image clone() const;
```

A new image of the same pixels in format `f` (a copy even to the same format), or in the same format.

### exif, icc, orientation, oriented

```cpp
slice<const byte> exif() const noexcept;
slice<const byte> icc() const noexcept;
uint8_t orientation() const noexcept;
image oriented() const;
```

The file's EXIF (a TIFF structure) and ICC profile, empty when it had none; EXIF's orientation, 1 to 8, 1 when the file said nothing; and the image turned and mirrored as the orientation says.

### save, async_save

```cpp
expected<void, error> save(const string& path, const save_options& o = {}) const;
async::task<expected<void, error>> async_save(const string& path, const save_options& o = {}) const;
```

The image written to a file in the format its extension names, through `path + ".part"` and a rename.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/core/core.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    // a row of red, green and blue, and a row of white
    codec::image picture(3, 2, codec::pixel_format::rgb8);
    const unsigned char values[] = {255, 0, 0, 0, 255, 0, 0, 0, 255,
                                    255, 255, 255, 255, 255, 255, 255, 255, 255};
    for (size_t i : range(sizeof values)) {
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

[`load`](README.md#files) and [`decode`](decode.md) for images from files; [`png`](png.md), [`jpeg`](jpeg.md) and [`gif`](gif.md) for each format.
