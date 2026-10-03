[sgcl](../../README.md) › [codec](../README.md) › [frames](../frames.md)

# sgcl::codec::frames::width

```cpp
uint32_t width() const noexcept;
```

The width of the canvas in pixels, as the file's header says it: a GIF's logical screen, a WebP's canvas. Every
frame's picture is this wide, whatever part of the canvas the frame itself covers. It is known when the frames are
made, before the first frame is read.

## Parameters

None.

## Return value

The width of the canvas.

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
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/gif_decode/welcome2.gif");
    codec::frames clip = codec::decode_frames(file);
    optional<codec::frame> first = clip.next();
    println("{} {}", clip.width(), first->picture.width());
}
```

Output:

```text
290 290
```

## See also

- [height](height.md): the height of the canvas
- [sgcl::codec::frames](../frames.md)
