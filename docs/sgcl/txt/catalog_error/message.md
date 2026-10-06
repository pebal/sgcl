[sgcl](../../README.md) › [txt](../README.md) › [catalog_error](README.md)

# sgcl::txt::catalog_error::message

```cpp
string message() const noexcept;
```

Returns why, in a few words.

## Parameters

None.

## Return value

The reason.

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
    println("{}", c.error().message());
}
```

Output:

```text
a string without its closing quote
```

## See also

- [catalog::parse_po](../catalog/parse_po.md)
- [sgcl::txt::catalog_error](README.md)
