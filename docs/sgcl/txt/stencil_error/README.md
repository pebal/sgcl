[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::stencil_error

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class stencil_error;
}
```

Where a source stopped being a template, and why, for whoever has to fix it: what [parse](../stencil/parse.md) answers
when a source is not a template, and what the `bad_expected_access<stencil_error>` of a
[constructor](../stencil/stencil.md) carries. A byte offset alone is no use to a person looking at a file, so the line
and the column come with it, both counted from one. Go's `template.Parse` answers an `error` whose text holds the
name of the template and the line; here the parts are apart.

## Rules

- Three numbers and a pointer to a reason the library keeps for the whole run of the program: it lives anywhere and
  is copied freely.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stencil_error.md) | makes the error |
| [offset](offset.md) | the byte the reading stopped on |
| [line](line.md) | its line, from 1 |
| [column](column.md) | its column, from 1 |
| [message](message.md) | why, in a few words |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto t = txt::stencil::parse("Hello, {{ name ");
    if (!t) {
        println("{}:{}: {}", t.error().line(), t.error().column(), t.error().message());
    }
    return 0;
}
```

Output:

```text
1:8: the action is not closed
```

## See also

- [parse](../stencil/parse.md): what answers one
- [stencil](../stencil/README.md)
