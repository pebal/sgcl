[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::catalog

```cpp
catalog() noexcept = default;
```

Constructs the empty catalog: every id comes back as it is (for a plural, the id when n is 1 and the plural id
otherwise), the rule the Germanic `n != 1`.

## Parameters

None.

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
    txt::catalog c;
    println("{} | {} | {}", c.translate("Open"), c.translate("file", "files", 2), c.size());
}
```

Output:

```text
Open | files | 0
```

## See also

- [parse_po](parse_po.md)
- [parse_mo](parse_mo.md)
- [sgcl::txt::catalog](README.md)
