[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::lens_make

```cpp
optional<string> lens_make() const noexcept;
```

The maker of the lens: EXIF's LensMake (0xA433), else XMP's `exifEX:LensMake`. The text as the file writes it, its trailing spaces and NULs trimmed (cameras pad some fields, `"Canon   "`);
EXIF's ASCII and its UTF-8 (type 129 of EXIF 3.0) read as UTF-8, and bytes that are not UTF-8 as Latin-1.

## Parameters

None.

## Return value

The text, or `nullopt` when neither block has it or it is blank.

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
    println("{}", camera.lens_make().value_or("none"));
    println("{}", edited.lens_make().value_or("none"));
}
```

Output:

```text
Canon
Nikon
```

## See also

- [lens_model](lens_model.md): the lens
- [sgcl::codec::metadata](README.md)
