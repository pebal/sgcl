[sgcl](../../README.md) › [txt](../README.md) › [catalog_error](README.md)

# sgcl::txt::catalog_error::column

```cpp
size_t column() const noexcept;
```

Returns the column, in bytes from 1, the reading stopped on; 0 for a .mo.

## Parameters

None.

## Return value

The column.

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
    println("{}", c.error().column());
}
```

Output:

```text
8
```

## See also

- [catalog::parse_po](../catalog/parse_po.md)
- [sgcl::txt::catalog_error](README.md)
