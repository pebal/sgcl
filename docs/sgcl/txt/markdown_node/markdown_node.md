[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::markdown_node

```cpp
markdown_node() noexcept = default;
```

Constructs no node: [kind](kind.md) `document`, no children, empty texts; what a document gives is its nodes.

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

    txt::markdown_node none;
    println("{} {} [{}]", bool(none), none.size(), none.to_html());
}
```

Output:

```text
false 0 []
```

## See also

- [operator bool](operator_bool.md)
- [sgcl::txt::markdown_node](README.md)
