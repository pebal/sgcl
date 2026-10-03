[sgcl](../../README.md) › [txt](../README.md) › [decode_error](README.md)

# sgcl::txt::decode_error::offset

```cpp
size_t offset() const noexcept;
```

Returns the place of the first byte that means nothing, counted in bytes from the start of what was decoded: for a unit of UTF-16 or UTF-32 cut short, the first byte of that unit.

## Parameters

None.

## Return value

The offset.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    byte units[] = {byte('a'), byte(0), byte('b')};
    println("{}", txt::decode(units, txt::encoding::utf16le, txt::strict).error().offset());
}
```

Output:

```text
2
```

## See also

- [from](from.md)
- [sgcl::txt::decode_error](README.md)
