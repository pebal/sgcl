[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::orientation

```cpp
unsigned orientation() const noexcept;
```

How the picture is to be turned to stand upright, EXIF's Orientation (0x0112) of 1 to 8, else XMP's
`tiff:Orientation`: the value [image::orientation](../image/orientation.md) gives and
[image::oriented](../image/oriented.md) applies.

## Parameters

None.

## Return value

1 to 8; 1, the picture as stored, when neither block has a value in that range.

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
    codec::metadata camera = codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    codec::metadata edited = codec::metadata::load("tests/codec/fuzz/seeds/metadata/xmp.webp");
    println("{} and {}", camera.orientation(), edited.orientation());
    println("{}", codec::metadata().orientation());
}
```

Output:

```text
6 and 8
1
```

## See also

- [image::orientation](../image/orientation.md), [image::oriented](../image/oriented.md)
- [sgcl::codec::metadata](README.md)
