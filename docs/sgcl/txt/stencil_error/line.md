[sgcl](../../README.md) › [txt](../README.md) › [stencil_error](README.md)

# sgcl::txt::stencil_error::line

```cpp
size_t line() const noexcept;
```

The line of the source the reading stopped on, counted from 1: the new lines before the [offset](offset.md), and one.

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
    auto t = txt::stencil::parse("Dear {{ name }},\n{{ if late }}Sorry.\n");
    if (!t) {
        println("byte {}, line {}, column {}: {}", t.error().offset(), t.error().line(),
                t.error().column(), t.error().message());
    }
    return 0;
}
```

Output:

```text
byte 20, line 2, column 4: a block was left open
```

## See also

- [column](column.md)
- [sgcl::txt::stencil_error](README.md)
