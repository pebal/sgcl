[sgcl](../../README.md) › [txt](../README.md) › [stencil_functions](README.md)

# sgcl::txt::stencil_functions::builtin

```cpp
static const stencil_functions& builtin() noexcept;
```

The table of the six functions every template has, made once for the whole program and shared by every template
that does not ask for a table of its own: what [parse](../stencil/parse.md) (1) and the constructor of a
[stencil](../stencil/stencil.md) read with.

## Parameters

None.

## Return value

The table.

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
    const txt::stencil_functions& six = txt::stencil_functions::builtin();
    auto t = txt::stencil::parse("{{ . | title }}", six);
    println("{} {}", t->render("ada lovelace"), &six == &txt::stencil_functions::builtin());
    return 0;
}
```

Output:

```text
Ada Lovelace true
```

## See also

- [stencil_functions](stencil_functions.md): a table of one's own
- [sgcl::txt::stencil_functions](README.md)
