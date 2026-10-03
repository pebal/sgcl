[sgcl](../../README.md) › [codec](../README.md) › [webp](../webp.md)

# sgcl::codec::webp::frames

```cpp
/*(1)*/ static expected<codec::frames, error> frames(const slice<const byte>& data,
                                                     const decode_options& o = {}) noexcept;
/*(2)*/ static expected<codec::frames, error> frames(const io::reader& in,
                                                     const decode_options& o = {});
```

Opens a WebP as an animation: reads the RIFF header and VP8X, or the simple format's image header, and returns the
[frames](../frames.md), whose [next](../frames/next.md) decodes the frames one by one as the program asks for them. A
still image is one frame. Each [frame](../frame.md) is the whole canvas as it is shown, `rgba8` unless `o.want` asks
for another format.

1. Reads the file in memory, in place. The frames hold the bytes while they live: a slice of unmanaged memory must
   outlive them.
2. Reads the file from a stream as `next` asks.

The canvas is drawn by RFC 9649's rules. It starts transparent; the ANIM chunk's background color is a hint RFC 9649
lets a reader leave, and libwebp's `WebPAnimDecoder` and browsers leave it too. A frame disposed of is cleared to
transparent before the next one. A frame that blends is alpha-blended by RFC 9649's formula, in integers rounded to
the nearest: A = src.A + dst.A × (255 − src.A) / 255, and each color (src.C × src.A + dst.C × dst.A × (255 − src.A) /
255) / A, 0 when A is 0. A frame that does not blend is written over its rectangle.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted and the limits |

## Return value

The frames, or the error of what was read: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for
a file that is not a WebP or a header the class's [rules](../webp.md#rules) refuse, `errc::unexpected_end` for a file
that ends before its RIFF size, `errc::too_large` for a canvas past `o.limits.max_pixels`, and (2) `errc::io` when the
stream fails. An error in a frame comes from `next`, where it is found.

## Complexity

Constant: the headers before the first image. Each frame is decoded by `next`, linear in its bytes and in the pixels
of the canvas.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The composition of the canvas is the module's. The frames' own pixels are libwebp's bit for bit, but a blended pixel
may differ from `WebPAnimDecoder`'s. libwebp blends in fixed point: one off in alpha, and off in a color by up to
about 255 / A for a pixel of alpha A. That is one or two for most pixels, and more only where the pixel is nearly
transparent: its error in the background's weight is divided by A. The tests hold a blended pixel to alpha ± 1 and
color ± (1 + 255 / A) against libwebp; matching it exactly would mean taking its approximation for the formula.

The loop count is ANIM's: [loop_count](../frames/loop_count.md) is 0 for forever, `n` plays for `n`, 1 for a still
image. ANIM comes before the first frame and is read when the frames are made, so the count is known at once; a
second ANIM chunk is passed over, as libwebp's demuxer passes it.

The delay of a frame is its duration in milliseconds, 0 left as 0. The frames carry no metadata: their images'
[exif](../image/exif.md) and [icc](../image/icc.md) are empty; [decode](decode.md) reads them.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

#include <chrono>

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/webp_decode/made_animation.webp");
    codec::frames clip = codec::webp::frames(file);
    int count = 0;
    std::chrono::milliseconds total{0};
    while (optional<codec::frame> shown = clip.next()) {
        total += std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::nanoseconds(shown->delay));
        ++count;
    }
    println("animation: {}x{}, {} frames, {} ms, plays {}", clip.width(), clip.height(), count,
            total.count(), clip.loop_count());

    vector<byte> still = io::read_file("tests/codec/fuzz/seeds/webp_decode/test.webp");
    codec::frames one = codec::webp::frames(still);
    optional<codec::frame> first = one.next();
    optional<codec::frame> second = one.next();
    const bool rgba = first->picture.format() == codec::pixel_format::rgba8;
    println("still: rgba8: {}, a second frame: {}", rgba, second.has_value());
}
```

Output:

```text
animation: 16x12, 4 frames, 160 ms, plays 3
still: rgba8: true, a second frame: false
```

## See also

- [decode](decode.md): a still image, or the first frame alone
- [codec::decode_frames](../decode_frames.md): GIF or WebP, told by its signature
- [frames](../frames.md), [frame](../frame.md)
- [sgcl::codec::webp](../webp.md)
