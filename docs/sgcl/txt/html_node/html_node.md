[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::html_node

```cpp
html_node() noexcept = default;
```

Constructs no node: false as a truth, its children none, its text and names empty. A document's nodes come from
[html_document](../html_document/README.md).

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

    txt::html_node none;
    println("{} {} [{}]", bool(none), none.size(), none.outer_html());
}
```

Output:

```text
false 0 []
```

## See also

- [html_document::root](../html_document/root.md)
- [sgcl::txt::html_node](README.md)
