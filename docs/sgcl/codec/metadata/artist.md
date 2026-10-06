[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::artist

```cpp
optional<string> artist() const noexcept;
```

Who made the picture: EXIF's Artist (0x013B), else XMP's `dc:creator` or `tiff:Artist`; the creators of a list joined by "; ", as EXIF writes several and ImageIO reads them. The text as the file writes it, its trailing spaces and NULs trimmed (cameras pad some fields, `"Canon   "`);
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
    println("{}", camera.artist().value_or("none"));
    println("{}", edited.artist().value_or("none"));
}
```

Output:

```text
Jan Kowalski
Anna Nowak; Second
```

## See also

- [copyright](copyright.md): the rights
- [sgcl::codec::metadata](README.md)
