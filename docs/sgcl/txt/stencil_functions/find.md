[sgcl](../../README.md) › [txt](../README.md) › [stencil_functions](README.md)

# sgcl::txt::stencil_functions::find

```cpp
const stencil_function* find(const string& name) const noexcept;
```

The function of `name`, or null when the table has none of that name: the question [parse](../stencil/parse.md)
asks of every name a pipeline calls.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |

## Return value

The function, or null.

## Complexity

Constant on average.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::stencil_functions table;
    const txt::stencil_function* upper = table.find("upper");
    txt::value shouted = (*upper)(txt::value("quiet"), {});
    println("{} {}", shouted, table.find("whisper") == nullptr);
    return 0;
}
```

Output:

```text
QUIET true
```

## See also

- [add](add.md)
- [sgcl::txt::stencil_functions](README.md)
