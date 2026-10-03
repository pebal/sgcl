[sgcl](../../README.md) › [codec](../README.md) › [frames](README.md)

# sgcl::codec::frames::loop_count

```cpp
uint32_t loop_count() const noexcept;
```

How many times the animation plays: 0 for forever, 1 for once, which is also what a file that says nothing gives.
A GIF says it in its NETSCAPE2.0 (or ANIMEXTS1.0) extension, whose count n is n + 1 plays, the first and n more; a
WebP in its ANIM chunk, the plays themselves.

It is known once that extension is read. A GIF's, before its first image, is read when the frames are made, and so
is a WebP's ANIM chunk, which comes before its first frame: the count is there before the first [next](next.md). A
second ANIM chunk is passed over, as libwebp passes it.

## Parameters

None.

## Return value

The number of plays, 0 for forever.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* path : {"tests/codec/fuzz/seeds/gif_decode/fire.gif",
                             "tests/codec/fuzz/seeds/gif_decode/welcome2.gif",
                             "tests/codec/fuzz/seeds/gif_decode/treescap.gif"}) {
        vector<byte> file = io::read_file(path);
        println("{}", codec::decode_frames(file)->loop_count());
    }

    vector<byte> webp = io::read_file("tests/codec/fuzz/seeds/webp_decode/made_animation.webp");
    codec::frames clip = codec::decode_frames(webp);
    println("{}", clip.loop_count());
    optional<codec::frame> first = clip.next();
    println("{}", clip.loop_count());
}
```

Output:

```text
0
1001
1
3
3
```

## See also

- [next](next.md): the next frame
- [sgcl::codec::frames](README.md)
