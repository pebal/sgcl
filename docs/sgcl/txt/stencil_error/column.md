[sgcl](../../README.md) › [txt](../README.md) › [stencil_error](../stencil_error.md)

# sgcl::txt::stencil_error::column

```cpp
size_t column() const noexcept;
```

The column of the source the reading stopped on, counted from 1 in bytes from the start of its [line](line.md).

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

- [line](line.md)
- [sgcl::txt::stencil_error](../stencil_error.md)
