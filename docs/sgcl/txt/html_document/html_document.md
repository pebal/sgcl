[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::html_document

```cpp
html_document() noexcept = default;
```

Constructs no document: its [root](root.md) is no node and its [to_string](to_string.md) empty; what
[parse](parse.md) and [parse_fragment](parse_fragment.md) return is the document of a text.

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

    txt::html_document none;
    println("{} [{}]", bool(none.root()), none.to_string());
}
```

Output:

```text
false []
```

## See also

- [parse](parse.md)
- [sgcl::txt::html_document](README.md)
