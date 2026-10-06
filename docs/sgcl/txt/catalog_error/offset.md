[sgcl](../../README.md) › [txt](../README.md) › [catalog_error](README.md)

# sgcl::txt::catalog_error::offset

```cpp
size_t offset() const noexcept;
```

Returns the byte the reading stopped on.

## Parameters

None.

## Return value

The offset, from 0.

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
    auto c = txt::catalog::parse_po("msgid \"a\"\nmsgstr \"b\n");
    println("{}", c.error().offset());
}
```

Output:

```text
17
```

## See also

- [catalog::parse_po](../catalog/parse_po.md)
- [sgcl::txt::catalog_error](README.md)
