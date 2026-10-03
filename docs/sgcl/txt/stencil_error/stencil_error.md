[sgcl](../../README.md) › [txt](../README.md) › [stencil_error](README.md)

# sgcl::txt::stencil_error::stencil_error

```cpp
stencil_error(size_t offset, size_t line, size_t column, const char* reason) noexcept;
```

Makes the error of a source that stopped at byte `offset`, on `line` and `column`, for `reason`. The reason is not
copied: it is a text that lives as long as the program, a literal. The parser makes one; a program makes one for an
error of its own that sits beside the parser's, a check of a template it adds after parsing.

## Parameters

| Parameter | Description |
|---|---|
| `offset` | the byte the reading stopped on, from 0 |
| `line` | its line, from 1 |
| `column` | its column, from 1 |
| `reason` | why, a literal |

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
    txt::stencil_error e(12, 2, 5, "the letter has no signature");
    println("{}:{}: {} (byte {})", e.line(), e.column(), e.message(), e.offset());
    return 0;
}
```

Output:

```text
2:5: the letter has no signature (byte 12)
```

## See also

- [sgcl::txt::stencil_error](README.md)
