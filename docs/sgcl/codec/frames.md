[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::frames

```cpp
#include "sgcl/codec/frames.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class frames;
}
```

`sgcl::codec::frames` is the frames of an animation, GIF or WebP, read one by one as they are asked for. Each
[frame](frame.md) is a new [image](image.md) of the whole canvas, with what came before composed under it as the
format says: the disposal and transparency of GIF, the disposal and alpha blending of WebP. Nothing is decoded ahead
of [next](frames/next.md): the reading keeps the canvas, and a frame is the program's to keep or let go of. There is
no public constructor: [gif::frames](gif/frames.md) and [webp::frames](webp/frames.md) make one, and
[decode_frames](decode_frames.md) makes either, told by the signature.

The frames' pixels are those of the format's reference decoder bit for bit. WebP's blending onto the canvas is RFC
9649's exact formula, which libwebp approximates in fixed point, so a blended pixel may differ from libwebp's
([webp](webp.md) has the bounds). Go's `gif.DecodeAll` reads every frame at once and gives each as the rectangle it
covers, leaving the composing to the program; here the frames come composed, one at a time.

## Rules

- A handle of one word: a tracked pointer to the state of the reading, so it lives where a `tracked_ptr` may (on a
  stack, in a task, inside a managed object). Copies share the reading, and a frame read through one copy is not read
  again through another. A move copies the word, as a `tracked_ptr`'s does: the moved-from frames are another handle
  of the same reading.
- What it reads from lives while the frames do: the file's bytes, held (a slice of memory that is not managed must
  outlive the frames), or the stream, read as the frames are asked for.
- After the last frame, [next](frames/next.md) gives `nullopt`, and again on every call. An error of the data comes
  where it is found, and again on every call after; a file of no frame is `errc::corrupt` at the first call. A read
  of the stream that throws stops the reading: `errc::io` after it.
- [next](frames/next.md) moves the reading on: one thread at a time reads through a handle and its copies.

## Member functions

| Function | Description |
|---|---|
| [next](frames/next.md) | the next frame, decoded |

#### Observers

| Function | Description |
|---|---|
| [width](frames/width.md) | the width of the canvas |
| [height](frames/height.md) | the height of the canvas |
| [loop_count](frames/loop_count.md) | how many times the animation plays, 0 for forever |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/gif_decode/welcome2.gif");
    codec::frames clip = codec::gif::frames(file);
    codec::frames same = clip;  // shares the reading
    optional<codec::frame> first = clip.next();
    optional<codec::frame> second = same.next();
    println("{}x{}, plays {}", clip.width(), clip.height(), clip.loop_count());
    println("first {}x{}, second {}x{}", first->picture.width(), first->picture.height(),
            second->picture.width(), second->picture.height());
    int rest = 0;
    while (optional<codec::frame> shown = clip.next()) {
        ++rest;
    }
    println("{} frames after those two", rest);
}
```

Output:

```text
290x48, plays 1001
first 290x48, second 290x48
4 frames after those two
```

## See also

- [frame](frame.md): one frame
- [decode_frames](decode_frames.md): the frames of a GIF or a WebP, told by the signature
- [gif](gif.md), [webp](webp.md): the formats
- [codec](README.md)
