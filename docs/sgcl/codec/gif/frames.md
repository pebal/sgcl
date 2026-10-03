[sgcl](../../README.md) › [codec](../README.md) › [gif](README.md)

# sgcl::codec::gif::frames

```cpp
static expected<codec::frames, error> frames(const slice<const byte>& data,             // (1)
                                             const decode_options& o = {}) noexcept;
static expected<codec::frames, error> frames(const io::reader& in,                      // (2)
                                             const decode_options& o = {});
```

Opens a GIF as an animation: reads the header, the logical screen with its color table and the extensions before
the first image, and returns the [frames](../frames/README.md), whose [next](../frames/next.md) decodes the frames one by
one as the program asks for them. Each [frame](../frame.md) is the whole canvas as it is shown.

1. Reads the file in memory, in place. The frames hold the bytes while they live: a slice of unmanaged memory must
   outlive them.
2. Reads the file from a stream as `next` asks: the memory is the canvas and a block of the stream, never the whole
   file.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted and the limits; the default is `rgba8` and 100 million pixels |

## Return value

The frames, or the error of what was read: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt`
for a file that is not a GIF, `errc::unexpected_end` for one that ends before its first image, `errc::too_large` for a
logical screen past `o.limits.max_pixels`, and (2) `errc::io` when the stream fails. An error in a frame comes from
`next`, where it is found, and so does `errc::corrupt` for a file of no image, at its trailer.

## Complexity

Linear in the bytes of the header, the color table and the extensions before the first image. Each frame is
decoded by `next`, linear in its bytes and in the pixels of the canvas.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The loop count comes from NETSCAPE2.0 (or ANIMEXTS1.0) and counts how many times the animation plays:
[loop_count](../frames/loop_count.md) is 0 for forever, `n + 1` for the file's `n` (the first play and `n` more), 1
for a file that says nothing. The extensions before the first frame are read here, so `loop_count()` is known at
once for the files that put it there.

The delay of a frame is what the file says, in hundredths of a second, 0 as often as not. Browsers show 0 and 10 ms
as 100 ms; the module leaves that to the program.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

#include <chrono>

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/gif_decode/fire.gif");
    codec::frames clip = codec::gif::frames(file);
    println("{}x{}, plays {} (0: forever)", clip.width(), clip.height(), clip.loop_count());
    int count = 0;
    std::chrono::milliseconds total{0};
    while (optional<codec::frame> shown = clip.next()) {
        total += std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::nanoseconds(shown->delay));
        ++count;
    }
    println("{} frames, {} ms for one play", count, total.count());
}
```

Output:

```text
30x60, plays 0 (0: forever)
33 frames, 1650 ms for one play
```

## See also

- [decode](decode.md): the first frame alone
- [codec::decode_frames](../decode_frames.md): GIF or WebP, told by its signature
- [frames](../frames/README.md), [frame](../frame.md)
- [sgcl::codec::gif](README.md)
