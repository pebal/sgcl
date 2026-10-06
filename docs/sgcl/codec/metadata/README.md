[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::metadata

```cpp
#include "sgcl/codec/metadata.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class metadata;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::codec::metadata` is what a file says about its picture, typed: the camera and the lens, when it was taken,
the exposure, where, and how it stands. It reads EXIF (CIPA DC-008: IFD0, the Exif IFD and the GPS IFD) and XMP
(ISO 16684-1, the `tiff`, `exif`, `exifEX`, `aux`, `xmp`, `photoshop` and `dc` namespaces of Adobe's part 2) from
JPEG, PNG, WebP, TIFF, HEIF and AVIF without decoding a pixel, or from the EXIF block an
[image](../image/README.md) keeps. Go's standard library has no reader of EXIF; this is what ImageIO's properties of
a file give on macOS (what `sips` and `mdls` show), or exiftool's common fields.

Each field is EXIF's where the EXIF block has it and XMP's where it does not: the two agree in a file one program
wrote, and where they differ EXIF's is the camera's. A field is `nullopt` where neither block has it, or has it in a
form that does not read; the times are [time::datetime](../../time/datetime/README.md)s in the zone of the file's
offset, and the raw blocks stay at hand for the tags the type has no field for.

## Rules

- **Lenient inside the blocks**, as exiftool and ImageIO are: a field of a wrong type, a value past its block, a
  rational of denominator 0, an impossible date or a pointer out of the block leaves that field absent, and the rest
  is read. Pointers to IFDs are followed once; nothing loops.
- **Errors are values**: [read](read.md) and [load](load.md) return an [expected](../../core/expected/README.md) with
  an [error](../error/README.md), for bytes of no format the module reads, a block past the
  [limits](../limits.md) and a file or a stream that fails. A file without metadata is metadata of nothing.
- **A copy shares** the fields, which never change once read; it is a handle of one tracked word.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](metadata.md) | metadata of nothing |

#### Reading

| Function | Description |
|---|---|
| [from_exif](from_exif.md) | the metadata of an EXIF block and an XMP packet in hand (static) |
| [load](load.md) | the metadata of the file at a path (static) |
| [read](read.md) | the metadata of a file in memory or a stream (static) |

#### The camera and the picture

| Function | Description |
|---|---|
| [artist](artist.md) | who made the picture |
| [copyright](copyright.md) | the rights |
| [description](description.md) | what the picture shows |
| [lens_make](lens_make.md) | the maker of the lens |
| [lens_model](lens_model.md) | the lens |
| [make](make.md) | the maker of the camera |
| [model](model.md) | the model of the camera |
| [software](software.md) | the program that made or last wrote the file |

#### Time

| Function | Description |
|---|---|
| [date_time](date_time.md) | when the file was last changed |
| [date_time_digitized](date_time_digitized.md) | when the picture was stored as data |
| [date_time_original](date_time_original.md) | when the picture was taken |
| [gps_time](gps_time.md) | when the GPS fixed the position, in UTC |

#### Exposure

| Function | Description |
|---|---|
| [exposure_bias](exposure_bias.md) | the exposure compensation in EV |
| [exposure_time](exposure_time.md) | the exposure in seconds |
| [f_number](f_number.md) | the aperture |
| [flash_fired](flash_fired.md) | whether the flash fired |
| [focal_length](focal_length.md) | the focal length in millimetres |
| [focal_length_35mm](focal_length_35mm.md) | the focal length in 35 mm terms |
| [iso](iso.md) | the sensitivity |

#### Place and image

| Function | Description |
|---|---|
| [height](height.md) | the height as the metadata says it |
| [location](location.md) | where the picture was taken |
| [orientation](orientation.md) | how the picture is to be turned, 1 to 8 |
| [rating](rating.md) | the rating, −1 to 5 |
| [width](width.md) | the width as the metadata says it |

#### The blocks

| Function | Description |
|---|---|
| [exif](exif.md) | the EXIF block as the file holds it |
| [xmp](xmp.md) | the XMP packet as text |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    codec::metadata m = codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    println("{} {}, {}", m.make().value_or("?"), m.model().value_or("?"),
            m.lens_model().value_or("?"));
    println("taken {}", *m.date_time_original());
    println("1/{} s, f/{}, ISO {}, {} mm", 1 / *m.exposure_time(), *m.f_number(), *m.iso(),
            *m.focal_length());
    if (auto place = m.location()) {
        println("at {:.5f}, {:.5f}", place->latitude, place->longitude);
    }
}
```

Output:

```text
Canon Canon EOS R5, RF24-105mm F4 L IS USM
taken 2024-05-06T18:29:41.25+02:00
1/250 s, f/2.8, ISO 400, 105 mm
at -33.85980, 151.20843
```

## See also

- [location](../location.md): latitude, longitude and altitude
- [image::exif](../image/exif.md): the block a decoded image keeps
- [time::datetime](../../time/datetime/README.md)
- [encoding::xml](../../encoding/xml/README.md): the parser of the XMP packets
