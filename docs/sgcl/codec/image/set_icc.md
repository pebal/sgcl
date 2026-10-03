[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::set_icc

```cpp
void set_icc(const slice<const byte>& bytes) noexcept;
```

Sets the ICC profile of the image to a copy of `bytes`, which [icc](icc.md) then gives: the profile that says what
colors the pixels mean. An empty slice removes it. The module does not read or check the profile: it is the
program's, and [png::encode](../png/encode.md), [jpeg::encode](../jpeg/encode.md) and
[heif::encode](../heif/encode.md) write it into the file with the pixels as they are.

The image is a handle: every copy of the handle sees the new profile.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the ICC profile, or an empty slice for none |

## Return value

None.

## Complexity

Linear in the size of the profile.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image photo(9, 7, codec::pixel_format::rgb8);
    vector<byte> profile(300, byte('p'));  // the bytes of a profile, which the module does not read
    photo.set_icc(profile);
    codec::image kept = codec::png::decode(codec::png::encode(photo));
    println("{} bytes, {} through PNG", photo.icc().size(), kept.icc().size());
    photo.set_icc({});
    println("{} bytes", photo.icc().size());
}
```

Output:

```text
300 bytes, 300 through PNG
0 bytes
```

## See also

- [icc](icc.md): the ICC profile
- [set_exif](set_exif.md): sets the EXIF block
- [sgcl::codec::image](README.md)
