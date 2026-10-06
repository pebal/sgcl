[sgcl](../README.md) › codec

# sgcl::codec

```cpp
#include "sgcl/codec.h"   // namespace sgcl::codec
```

Images: what Go has in `image/png`, `image/jpeg`, `image/gif` and `x/image`'s BMP and TIFF, and what libpng,
libjpeg-turbo, giflib, libwebp and libtiff decode and encode in C. An [image](image/README.md) is a handle of one word to its size, its pixels in one of nine
[pixel formats](pixel_format.md) and the metadata of its file (EXIF and the ICC profile, as bytes);
[load](load.md) reads a file of any of the module's formats and [save](save.md) writes one in the format its
extension names, [decode](decode.md) does what `load` does for a file already in memory, and each format has a type
of its own: [png](png/README.md), [jpeg](jpeg/README.md), [gif](gif/README.md), [webp](webp/README.md), [heif](heif/README.md),
[tiff](tiff/README.md), [bmp](bmp/README.md), [ico](ico/README.md), [qoi](qoi/README.md) and [pnm](pnm/README.md);
[jxl](jxl/README.md) reads JPEG XL through the system's codec, [qr](qr/README.md) makes QR codes, and
[metadata](metadata/README.md) reads a file's EXIF and XMP, typed, without decoding it. The module depends
on [core](../core/README.md), [io](../io/README.md) (the files and the streams read and written),
[async](../async/README.md) (`async_load` and `async_save` on the blocking pool), [compress](../compress/README.md)
(zlib's DEFLATE for PNG and TIFF, LZW for GIF), [hash](../hash/README.md) (CRC-32 and Adler-32 for PNG),
[time](../time/README.md) (the times of the metadata) and [encoding](../encoding/README.md) (XMP's RDF/XML). The index of the
whole interface is [the modules](../README.md).

Every format is written from its specification: PNG 3rd ed. (W3C), T.81 with JFIF 1.02 and Adobe's APP14, GIF89a,
RFC 9649 and RFC 6386 for WebP, TIFF 6.0 with its technical notes, Microsoft's documentation of BMP, ICO and CUR,
the QOI specification 1.0, Netpbm's pages of PBM, PGM, PPM and PAM, CIPA DC-008 for the orientation of EXIF. The reference libraries (libpng,
libjpeg-turbo, giflib, libwebp) and Go's image packages are the oracles of the tests and nothing more: PNG decodes
pixel for pixel as libpng and Go do and its encoder's filters are libpng's row for row; JPEG decodes bit for bit as
`djpeg -dct int`, and its encoder writes what `cjpeg -dct int -baseline` writes, byte for byte; GIF's frames are
giflib's and Go's pixel for pixel; WebP decodes to libwebp's pixels byte for byte, its container taken and refused as
libwebp's demuxer takes and refuses it; TIFF, BMP, ICO, QOI and the Netpbm formats decode as ffmpeg and ImageIO
decode them, pixel for pixel. HEIF and AVIF are not decoded by the module: HEVC and AV1 come from the
system (ImageIO on macOS), and the module's wrapping is held against ImageIO's own reading of each file.

A file is data from outside, and what is wrong in it is a value, never an exception: every decoder returns an
[expected](../core/expected/README.md) of the image and a [codec::error](error/README.md), the code and the byte of the file where
it was found. A small file can claim a huge image, so every decoder checks the size it declares against the
[limits](limits.md) before it allocates anything. The decoders are fuzzed (libFuzzer with ASan and UBSan, a harness
for each); their code has not been audited on its own, so a program takes files from outside with the limits set
to what it expects.

## The rules

1. An [image](image/README.md) and [frames](frames/README.md) are handles of one tracked word: they live where a `tracked_ptr`
   may ([the rules of core](../core/README.md#the-rules), 1), copies share the pixels or the reading, and `clone()`
   makes new pixels. A slice of an image's pixels, from `pixels()` or `row()`, keeps them alive after the image is
   gone. An [error](error/README.md) holds a `string` and lives where a string may.
2. The data of a file never throws: a decoder, `load`, `decode` and `decode_frames` return
   `expected<T, codec::error>`, an encoder of bytes `expected<vector<byte>, codec::error>`, an encoder into a stream
   and `save` `expected<void, codec::error>`.
   `codec::image photo = codec::load(path);` takes the image and throws `bad_expected_access<codec::error>` when
   the file does not load; a result that is not looked at fails quietly. What throws is a contract of the
   program: a side of zero or a pixel format outside the list (`invalid_argument`), a row past the last
   (`out_of_range`), a JPEG quality outside 1..100 or subsampling outside the list (`invalid_argument`), an image
   larger than an address holds
   (`length_error`; a file that claims one is `errc::too_large`, whatever `limits.max_pixels` says).
3. Without [decode_options](decode_options.md)`::want` an image comes in its file's own pixel format: a gray PNG
   stays `gray8` or `gray16`, a JPEG is `rgb8`, `gray8` or `cmyk8`, a GIF frame is `rgba8`, a WebP `rgba8` or
   `rgb8` as it says it has alpha or not. `want` asks for one format whatever the file holds, each row converted
   as it is decoded. Alpha is always straight, and no decoder turns an image by its EXIF orientation:
   [oriented](image/oriented.md) does.
4. Every `decode` also takes an [io::reader](../io/reader/README.md) and reads the file as it comes, so that memory is
   the image and a constant, not the file; the exceptions are a progressive JPEG, whose coefficients are kept
   whole until its last scan, a lossy WebP's VP8 chunk, read whole, HEIF, read to the end of the stream before
   the system's codec sees it, and TIFF and ICO, whose directories point anywhere in the file, read to its end. Every `encode` also writes into an [io::writer](../io/writer/README.md) and returns
   `expected<void, codec::error>`, `errc::io` when the stream fails.
5. Decoding and encoding run on the calling thread, as long as the image takes. A task that loads or saves a file
   calls [async_load](load.md) and [async_save](save.md), which run on the blocking pool; other work of the module
   in a task goes there through [spawn_blocking](../async/spawn_blocking.md), so that it does not hold a worker.
6. HEIC, HEIF, AVIF and JPEG XL are read, and HEIC written, through the system's codec: ImageIO on macOS. Elsewhere
   every call of [heif](heif/README.md) and [jxl](jxl/README.md) is `errc::unsupported`, and so is `load` or `decode` of
   such a file.

### The formats

| Format | Read | Write | Description |
|---|---|---|---|
| [AVIF](heif/README.md) | macOS | no | through the platform's ImageIO |
| [BMP](bmp/README.md) | yes | yes | every header and depth, bit fields, RLE4 and RLE8 read; 8, 24 or 32 bits written |
| [GIF](gif/README.md) | yes | yes | GIF87a and GIF89a; animations through `frames`; written as GIF89a, a palette made for each frame |
| [HEIC](heif/README.md) | macOS | macOS | through the platform's ImageIO |
| [ICO, CUR](ico/README.md) | yes | yes | icons and cursors, BMP and PNG entries |
| [JPEG](jpeg/README.md) | yes | yes | baseline, extended and progressive read; baseline written |
| [JPEG XL](jxl/README.md) | macOS | no | through the platform's ImageIO |
| [PBM, PGM, PPM, PAM](pnm/README.md) | yes | yes | Netpbm's formats, plain and raw |
| [PNG](png/README.md) | yes | yes | every type and depth, Adam7 |
| [QOI](qoi/README.md) | yes | yes | RGB and RGBA, lossless |
| [TIFF](tiff/README.md) | yes | yes | strips and tiles, none, PackBits, LZW, Deflate and JPEG read, several pages; none, LZW or Deflate written |
| [WebP](webp/README.md) | yes | yes | lossless, lossy, alpha, animations |

`save` and `image::save` of a format without an encoder give `errc::unsupported`; `load` and `decode` read every
format of the table, and `decode_all` of [tiff](tiff/README.md) and [ico](ico/README.md) every page or entry.

## Functions

| Function | Header | Description |
|---|---|---|
| [codec_category](codec_category.md) | `error.h` | the `std::error_category` of an `errc` |
| [decode](decode.md) | `decode.h` | the image of a file in any of the module's formats, from bytes or a stream, told by its signature |
| [decode_frames](decode_frames.md) | `decode.h` | the frames of an animation, GIF or WebP, told by its signature |
| [load, async_load](load.md) | `files.h` | the image of the file at a path, in any of the module's formats |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as a `std::error_code` |
| [save, async_save](save.md) | `files.h` | an image written to a file in the format its extension names |
| [sniff](sniff.md) | `format.h` | the format a file's first bytes claim |

## Classes

| Class | Header | Description |
|---|---|---|
| [bmp](bmp/README.md) | `bmp.h` | BMP: every header from BITMAPCOREHEADER to V5, every depth, bit fields, RLE; read and written |
| [decode_options](decode_options.md) | `options.h` | what a decoding asks for: the pixel format, the limits, the metadata or not |
| [error](error/README.md) | `error.h` | what went wrong in a file, and at which byte |
| [frame](frame.md) | `frames.h` | a frame of an animation: the whole canvas and how long it is shown |
| [frames](frames/README.md) | `frames.h` | the frames of an animation, each the whole canvas, read one by one |
| [gif](gif/README.md) | `gif.h` | GIF87a and GIF89a: the first frame, or all of them; an image or an animation written |
| [heif](heif/README.md) | `heif.h` | HEIC, HEIF and AVIF through the system's codec: the first image read, HEIC written |
| [ico](ico/README.md) | `ico.h` | ICO and CUR: the largest entry or all of them, BMP and PNG inside; one image or several written |
| [image](image/README.md) | `image.h` | an image in memory: its size, its pixel format, its rows, EXIF and ICC |
| [jpeg](jpeg/README.md) | `jpeg.h` | JPEG: baseline, extended and progressive read, every sampling, CMYK and YCCK; baseline written |
| [jxl](jxl/README.md) | `jxl.h` | JPEG XL through the system's codec: the first frame read |
| [limits](limits.md) | `options.h` | how many pixels a file may claim and how much metadata it may carry |
| [location](location.md) | `metadata.h` | where a photo was taken: latitude, longitude, altitude |
| [metadata](metadata/README.md) | `metadata.h` | a file's EXIF and XMP, typed: camera, lens, times, exposure, place, orientation |
| [png](png/README.md) | `png.h` | PNG: every type and depth, Adam7, tRNS, eXIf, iCCP; written with adaptive filters |
| [pnm](pnm/README.md) | `pnm.h` | PBM, PGM, PPM and PAM, plain and raw; read and written |
| [qoi](qoi/README.md) | `qoi.h` | QOI: RGB and RGBA, lossless; read and written |
| [qr](qr/README.md) | `qr.h` | QR codes (ISO/IEC 18004): the four modes, versions 1 to 40, drawn into an image or SVG |
| [save_options](save_options.md) | `files.h` | what `save` writes: the PNG level, the JPEG, HEIC and WebP quality, the JPEG subsampling, lossless WebP |
| [tiff](tiff/README.md) | `tiff.h` | TIFF: strips and tiles, five compressions, gray, palette, RGB, CMYK, several pages; read and written |
| [webp](webp/README.md) | `webp.h` | WebP: the container, lossless and lossy images, alpha, animations; read and written |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the kinds of failure, one list for every format |
| [flip](flip.md) | `image.h` | which way `image::flipped` mirrors |
| [format](format.md) | `format.h` | the file formats the module reads, told by their signatures |
| [pixel_format](pixel_format.md) | `image.h` | how the pixels of an image lie in its rows: nine layouts |

## See also

- [Benchmarks](benchmarks.md): each format against libpng, libjpeg-turbo, libwebp, giflib and Go
- [compress](../compress/README.md): DEFLATE and LZW under PNG and GIF
- [io](../io/README.md): the files and the streams
- [spawn_blocking](../async/spawn_blocking.md): decoding and encoding from a task
- [The modules](../README.md)
