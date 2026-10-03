[sgcl](../../README.md) › [txt](../README.md) › [byte_order_mark](README.md)

# sgcl::txt::byte_order_mark::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the bytes began with a byte order mark: `says.has_value()`.

## Parameters

None.

## Return value

`true` when there was a mark.

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
    byte marked[] = {byte(0xFE), byte(0xFF), byte(0), byte('a')};
    byte plain[] = {byte('a')};
    println("{} {}", bool(txt::detect_bom(marked)), bool(txt::detect_bom(plain)));
}
```

Output:

```text
true false
```

## See also

- [detect_bom](../detect_bom.md)
- [sgcl::txt::byte_order_mark](README.md)
