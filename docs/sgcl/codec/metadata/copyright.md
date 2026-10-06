[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::copyright

```cpp
optional<string> copyright() const noexcept;
```

The rights: EXIF's Copyright (0x8298), else XMP's `dc:rights` in its default language (`x-default`, else the first) or `tiff:Copyright`. The text as the file writes it, its trailing spaces and NULs trimmed (cameras pad some fields, `"Canon   "`);
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
    println("{}", camera.copyright().value_or("none"));
    println("{}", edited.copyright().value_or("none"));
}
```

Output:

```text
(c) 2024 Jan Kowalski
All rights
```

## See also

- [artist](artist.md): who made it
- [sgcl::codec::metadata](README.md)
