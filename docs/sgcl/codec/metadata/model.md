[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::model

```cpp
optional<string> model() const noexcept;
```

The model of the camera: EXIF's Model (0x0110), else XMP's `tiff:Model`. The text as the file writes it, its trailing spaces and NULs trimmed (cameras pad some fields, `"Canon   "`);
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
    println("{}", camera.model().value_or("none"));
    println("{}", edited.model().value_or("none"));
}
```

Output:

```text
Canon EOS R5
Z 9
```

## See also

- [make](make.md): the maker
- [sgcl::codec::metadata](README.md)
