[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::rating

```cpp
optional<int> rating() const noexcept;
```

The rating given to the picture in a catalogue: XMP's `xmp:Rating`, 0 to 5 stars, −1 for rejected; EXIF's Rating
(0x4746, written by Windows) first when it is there. A value outside −1 to 5 is `nullopt`; a fraction is taken
down to its whole stars.

## Parameters

None.

## Return value

The rating, or `nullopt`.

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
    println("{}", camera.rating().has_value());
    println("{} stars", *edited.rating());
}
```

Output:

```text
false
4 stars
```

## See also

- [xmp](xmp.md): the packet the rating comes from
- [sgcl::codec::metadata](README.md)
