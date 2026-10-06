[sgcl](../../README.md) › [txt](../README.md) › [markdown_document](README.md)

# sgcl::txt::markdown_document::markdown_document

```cpp
markdown_document() noexcept = default;
```

Constructs no document: its [root](root.md) is no node and its [to_html](to_html.md) empty.

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

    txt::markdown_document none;
    println("{} [{}]", bool(none.root()), none.to_html());
}
```

Output:

```text
false []
```

## See also

- [parse](parse.md)
- [sgcl::txt::markdown_document](README.md)
