[sgcl](../../README.md) › [txt](../README.md) › [catalog_error](README.md)

# sgcl::txt::catalog_error::line

```cpp
size_t line() const noexcept;
```

Returns the line the reading stopped on, from 1; 0 for a .mo.

## Parameters

None.

## Return value

The line.

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
    println("{}", c.error().line());
}
```

Output:

```text
2
```

## See also

- [catalog::parse_po](../catalog/parse_po.md)
- [sgcl::txt::catalog_error](README.md)
