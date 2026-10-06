[sgcl](../../README.md) › [codec](../README.md) › [pnm](README.md)

# sgcl::codec::pnm::encode

```cpp
static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept;    // (1)
static expected<void, error> encode(const image& im, const io::writer& out,                      // (2)
                                    const options& o = {});
```

Encodes an image as a Netpbm file of the [kind](../pnm-kind.md) `o.kind` names.

1. Returns the file as bytes.
2. Writes the file into a stream.

`kind::automatic` writes PGM for a gray image, PAM for one with alpha and PPM for any other. The samples are
written at the image's depth, maxval 255 for 8 bits and 65535 for 16 (big-endian), converted to the kind's
channels by [convert](../image/convert.md)'s rules: PGM and PPM drop alpha, PAM keeps the image's own channels
(`GRAYSCALE`, `GRAYSCALE_ALPHA`, `RGB` or `RGB_ALPHA`, `cmyk8` as RGB). PBM is a pixel black where its gray is
below 128 of 255, with no dithering. With `o.plain` PBM, PGM and PPM are written as numbers in text (P1, P2, P3),
70 characters a line at most, as Netpbm's own tools write them.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `out` | the stream the file is written into |
| `o` | the kind and the plain form; the default is the kind of the image's format, raw |

## Return value

1. The bytes of the file, or the [error](../error/README.md) `errc::invalid_argument` for a PAM asked plain, which
   PAM has no form of, or a kind outside the list.
2. Nothing, or the error: `errc::invalid_argument` as (1), before anything is written; `errc::io` when the stream
   fails, at the offset of the bytes written before, the stream's own error in [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The memory is a row converted and a row of output, made once; (1) adds the file, (2) writes a row at a time.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(2, 2, codec::pixel_format::rgba8);
    vector<byte> pam = codec::pnm::encode(picture);
    vector<byte> ppm = codec::pnm::encode(picture, {.kind = codec::pnm::kind::ppm});
    println("PAM {} bytes, PPM {} bytes", pam.size(), ppm.size());

    vector<byte> pbm = codec::pnm::encode(picture, {.kind = codec::pnm::kind::pbm, .plain = true});
    print("{}", string(reinterpret_cast<const char*>(pbm.data()), pbm.size()));

    expected<vector<byte>, codec::error> plain = codec::pnm::encode(picture, {.plain = true});
    println("{}", plain.error().message());
}
```

Output:

```text
PAM 81 bytes, PPM 23 bytes
P1
2 2
1 1
1 1
offset 0: pnm: PAM has no plain form
```

## See also

- [decode](decode.md): the image of a Netpbm file
- [kind](../pnm-kind.md), [options](../pnm-options.md)
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::pnm](README.md)
