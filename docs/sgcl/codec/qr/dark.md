[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::dark

```cpp
bool dark(uint32_t x, uint32_t y) const;
```

Whether the module at column `x`, row `y` of the symbol is dark, (0, 0) the top left corner, the quiet zone not counted.

## Parameters

| Parameter | Description |
|---|---|
| `x` | the column, 0 to `size() − 1` |
| `y` | the row, 0 to `size() − 1` |

## Return value

`true` for a dark module.

## Complexity

Constant.

## Exceptions

`out_of_range` for a module past the symbol.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::qr code = codec::qr::encode("SGCL");
    println("{} {} {}", code.dark(0, 0), code.dark(7, 0), code.dark(8, code.size() - 8));
}
```

Output:

```text
true false true
```

## See also

- [size](size.md)
- [to_image](to_image.md), [to_svg](to_svg.md)
- [sgcl::codec::qr](README.md)
