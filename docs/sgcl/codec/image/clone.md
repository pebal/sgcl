[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::clone

```cpp
image clone() const noexcept;
```

A new image of the same sides, format, pixels and metadata, with pixels of its own. A copy of an `image` is a
second handle of the same pixels, so a write through one is seen through the other; a clone is what a program
writes into while it keeps the original as it was.

## Parameters

None.

## Return value

The new image.

## Complexity

Linear in the bytes of the pixels and the metadata.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image original(2, 2, codec::pixel_format::gray8);
    codec::image copy = original;
    codec::image draft = original.clone();
    copy.pixels()[0] = byte(200);
    draft.pixels()[0] = byte(50);
    println("{}", std::to_integer<int>(original.pixels()[0]));
}
```

Output:

```text
200
```

## See also

- [convert](convert.md): a copy in another format
- [sgcl::codec::image](README.md)
