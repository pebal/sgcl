# sgcl::codec::frames, frame

```cpp
#include "sgcl/codec/frames.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    struct frame {
        image picture;     // the whole canvas as it is shown
        duration delay;    // as the file says
    };

    class frames {
    public:
        expected<optional<frame>, error> next();   // nullopt after the last frame
        uint32_t width() const noexcept;           // the canvas
        uint32_t height() const noexcept;
        uint32_t loop_count() const noexcept;      // how many times it plays: 0 forever
    };
}
```

The frames of an animation, read one by one as they are asked for. Each frame is a new [`image`](image.md) of the whole canvas, with what came before composed under it as the format says. [`gif::frames`](gif.md) and [`webp::frames`](webp.md) make them, and [`codec::decode_frames`](decode.md) makes either, told by the signature. The frames' decoded pixels match the format's reference decoder bit for bit; WebP's blending onto the canvas is RFC 9649's exact formula, which libwebp approximates, so a blended pixel may differ from libwebp's ([webp](webp.md) has the bounds).

- **A handle of one word.** Copies share the reading, and a frame read through one copy is not read again through another.
- **The end and errors.** After the last frame `next()` gives `nullopt`, and does again on every call. An error of the data comes where it is found and again on every call after.
- **What it holds.** The file's bytes (a slice of unmanaged memory must outlive the frames), or the stream, which is read as frames are asked for.

## Members

### frame

```cpp
struct frame {
    image picture;     // the whole canvas as it is shown
    duration delay;    // as the file says
};
```

One frame: the canvas with this frame drawn on what came before, and how long it is shown.

### next

```cpp
expected<optional<frame>, error> next();
```

The next frame, decoded when it is asked for; `nullopt` after the last, and again on every call. An error of the data comes where it is found, and again on every call after.

### width, height, loop_count

```cpp
uint32_t width() const noexcept;
uint32_t height() const noexcept;
uint32_t loop_count() const noexcept;
```

The size of the canvas, and how many times the animation plays, 0 for forever ([gif](gif.md) and [webp](webp.md) say when each knows it).

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

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

[`gif`](gif.md), [`image`](image.md).
