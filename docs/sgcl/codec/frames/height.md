[sgcl](../../README.md) › [codec](../README.md) › [frames](../frames.md)

# sgcl::codec::frames::height

```cpp
uint32_t height() const noexcept;
```

The height of the canvas in pixels, as the file's header says it: a GIF's logical screen, a WebP's canvas. Every
frame's picture is this high, whatever part of the canvas the frame itself covers. It is known when the frames are
made, before the first frame is read.

## Parameters

None.

## Return value

The height of the canvas.

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
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/webp_decode/made_animation.webp");
    codec::frames clip = codec::decode_frames(file);
    optional<codec::frame> first = clip.next();
    println("{} {}", clip.height(), first->picture.height());
}
```

Output:

```text
12 12
```

## See also

- [width](width.md): the width of the canvas
- [sgcl::codec::frames](../frames.md)
