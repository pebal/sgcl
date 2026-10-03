[sgcl](../../README.md) › [txt](../README.md) › [stencil_functions](../stencil_functions.md)

# sgcl::txt::stencil_functions::stencil_functions

```cpp
stencil_functions() noexcept;
```

A table of the six functions every template has — `upper`, `lower`, `title`, `trim`, `escape_html` and `default` —
to which the program's own are [added](add.md).

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
    txt::stencil_functions table;
    for (auto name : {"upper", "escape_html", "default", "shout"}) {
        print("{}:{} ", name, table.find(name) != nullptr);
    }
    println("");
    return 0;
}
```

Output:

```text
upper:true escape_html:true default:true shout:false 
```

## See also

- [builtin](builtin.md): the same six, shared
- [sgcl::txt::stencil_functions](../stencil_functions.md)
