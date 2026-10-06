[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::flash_fired

```cpp
optional<bool> flash_fired() const noexcept;
```

Whether the flash fired: bit 0 of EXIF's Flash (0x9209), else the field `Fired` of XMP's `exif:Flash` structure (an
element or an attribute of it). The other bits (the mode, the return light, red-eye reduction) are not read.

## Parameters

None.

## Return value

`true` or `false`, or `nullopt` when neither block says.

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
    println("{}", *camera.flash_fired());
    println("{}", *edited.flash_fired());
}
```

Output:

```text
true
false
```

## See also

- [exposure_time](exposure_time.md): the exposure
- [sgcl::codec::metadata](README.md)
