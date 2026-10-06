[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::lens_model

```cpp
optional<string> lens_model() const noexcept;
```

The lens: EXIF's LensModel (0xA434), else XMP's `exifEX:LensModel` or `aux:Lens`, the name Lightroom writes it under. The text as the file writes it, its trailing spaces and NULs trimmed (cameras pad some fields, `"Canon   "`);
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
    println("{}", camera.lens_model().value_or("none"));
    println("{}", edited.lens_model().value_or("none"));
}
```

Output:

```text
RF24-105mm F4 L IS USM
NIKKOR Z 400mm f/2.8 TC VR S
```

## See also

- [lens_make](lens_make.md): its maker
- [sgcl::codec::metadata](README.md)
