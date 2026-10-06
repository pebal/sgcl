[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::catalog_error

```cpp
#include "sgcl/txt/catalog.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class catalog_error;
}
```

`sgcl::txt::catalog_error` is where a .po or a .mo file stopped being one, for whoever has to fix it: the byte, the
line and the column (both 0 for a .mo, which has no lines) and why.

## Member functions

| Function | Description |
|---|---|
| [offset](offset.md) | the byte the reading stopped on |
| [line](line.md) | its line, from 1 |
| [column](column.md) | its column, from 1 |
| [message](message.md) | why |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto c = txt::catalog::parse_po("msgid \"a\"\n\nmsgid \"b\"\nmsgstr \"B\"\n");
    println("{}:{}: {}", c.error().line(), c.error().column(), c.error().message());
}
```

Output:

```text
3:1: msgid out of place
```

## See also

- [catalog](../catalog/README.md)
- [sgcl::txt](../README.md)
