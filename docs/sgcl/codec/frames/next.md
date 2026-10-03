[sgcl](../../README.md) › [codec](../README.md) › [frames](../frames.md)

# sgcl::codec::frames::next

```cpp
expected<optional<frame>, error> next();
```

Decodes the next frame and gives it: the canvas with the frame drawn on what came before, a new
[image](../image.md), and how long it is shown ([frame](../frame.md)). The frame is read when it is asked for, from
the bytes or the stream the frames hold. Copies of the handle share the reading: a frame read through one copy is not
read again through another.

After the last frame it gives `nullopt`, and again on every call. An error of the data comes where it is found, and
again on every call after: the frames before it were good, and the reading stops there. A file of no frame, GIF or
WebP, is `errc::corrupt` at the first call, where its end is found, as [decode](../decode.md) of it is.

When the read of the stream throws, the exception passes through `next`, and the reading stops where the stream left
it: every call after it gives `errc::io`, without an [io_error](../error/io_error.md), since the stream gave no error
of its own.

## Parameters

None.

## Return value

The next frame; `nullopt` after the last; or the [error](../error.md) of the data where it is found, at the byte
where it was found: `errc::corrupt`, also for a file of no frame, `errc::unexpected_end` for a file cut short,
`errc::too_large` for metadata past the limits, and `errc::io` when the stream fails, with io's error inside, and
after a read of the stream that threw.

## Complexity

Linear in the data of the frame and in the size of the canvas, which the new image copies.

## Exceptions

What the read of the stream throws, for frames read from a stream: an `io::reader` calls the read of the object it
holds. The reading stops there: the calls after it give `errc::io`. None for frames of bytes in memory.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/gif_decode/welcome2.gif");
    codec::frames whole = codec::decode_frames(file);
    int count = 0;
    while (optional<codec::frame> shown = whole.next()) {
        ++count;
    }
    println("{} frames, then nullopt: {}", count, !whole.next()->has_value());

    file.resize(file.size() / 2);
    codec::frames cut = codec::decode_frames(file);
    count = 0;
    expected<optional<codec::frame>, codec::error> step = cut.next();
    while (step && *step) {
        ++count;
        step = cut.next();
    }
    println("{} frames, then {}", count, step.error().message());
    println("again: {}", cut.next().error().message());
}
```

Output:

```text
6 frames, then nullopt: true
3 frames, then offset 18416: gif: the data ends in the middle
again: offset 18416: gif: the data ends in the middle
```

## See also

- [loop_count](loop_count.md): how many times the animation plays
- [frame](../frame.md): what it gives
- [sgcl::codec::frames](../frames.md)
