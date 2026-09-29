# sgcl::codec::gif

```cpp
#include "sgcl/codec/gif.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    class gif {
    public:
        // the first frame
        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {});

        // every frame, read one by one
        static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {});
        static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {});
    };
}
```

GIF87a and GIF89a.

**Frames.** Each frame is the whole canvas (the logical screen) as it is shown, `rgba8` unless [`want`](README.md#limits) asks for another format. The canvas starts transparent. A frame's own pixels are drawn over what the frames before left, the pixels of its transparent index are left as they were, and an interlaced frame's rows are put in their places. After a frame is shown, its disposal is done before the next frame is drawn:

- 0 and 1 leave the frame as it is;
- 2 clears its rectangle to transparent (as browsers do; the background color is not used);
- 3 puts back what was under it;
- 4 to 7 act as 1.

`decode` gives the first frame, and [`frames`](frames.md) all of them.

**The loop count** comes from NETSCAPE2.0 (or ANIMEXTS1.0) and counts how many times the animation plays: 0 forever, `n + 1` for the file's `n` (the first play and `n` more), 1 for a file that says nothing. `frames` reads the extensions before the first frame when it opens the file, so `loop_count()` is known at once for the files that put it there.

**The delay** is what the file says, in hundredths of a second, 0 as often as not. Browsers show 0 and 10 ms as 100 ms; the module leaves that to the program.

**What decoding checks.** The LZW data is compress's [LZW](../compress/lzw.md) with the deferred clear GIF encoders use.

- **Strict, as Go is.** An unknown block, a code past the LZW table, an image with fewer pixels than its size and no color table are `errc::corrupt`; data that ends early is `errc::unexpected_end`.
- **Lenient.** Pixels past an image's size are dropped (giflib does the same; Go refuses the file). An index past the palette is opaque black, and a frame reaching past the canvas is clipped to it.

**Memory.** The canvas, the indices of the largest frame so far, and for disposal 3 a copy of the canvas made the first time it is needed. Nothing is allocated for each frame but the image handed out.

## Members

### decode

```cpp
static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});
```

The first frame, as the whole canvas, from the file's bytes or from a stream.

### frames

```cpp
static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {});
static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {});
```

Every frame, read one by one as [`frames`](frames.md) asks for them; the extensions before the first frame are read at once, the loop count among them.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

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

[`frames`](frames.md), [`image`](image.md), [`decode`](decode.md), [`error`](error.md).
