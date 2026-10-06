[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::xmp

```cpp
string xmp() const noexcept;
```

The XMP packet the fields were read from, as text: JPEG's APP1 of `http://ns.adobe.com/xap/1.0/`, PNG's iTXt of
`XML:com.adobe.xmp` (decompressed), WebP's `XMP ` chunk, TIFF's tag 700, HEIF's item of type
`application/rdf+xml`. Its padding is kept, its trailing NULs are not. JPEG's extended XMP (packets past 64 KB in
further segments) is not read. A program that wants more of it parses it with
[encoding::xml](../../encoding/xml/README.md), as the type does.

## Parameters

None.

## Return value

The packet, or an empty string.

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
    string packet = edited.xmp();
    println("{}", packet.starts_with("<?xpacket"));
    println("{}", camera.xmp().empty());
}
```

Output:

```text
true
true
```

## See also

- [exif](exif.md): the other block
- [sgcl::codec::metadata](README.md)
