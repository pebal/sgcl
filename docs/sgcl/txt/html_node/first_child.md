[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::first_child

```cpp
html_node first_child() const noexcept;
```

Returns the first child.

## Parameters

None.

## Return value

The child; no node without children.

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
    auto doc = txt::html_document::parse(
        "<!DOCTYPE html><title>News</title><p id=lead>Hello "
        "<a href=\"/a\">first</a> and <a href=\"https://x.org/b\">second</a>. "
        "<template><b>later</b></template><svg viewbox=\"0 0 9 9\"><circle r=4></svg>");
    println("[{}]", doc.root().element_by_id("lead").first_child().data());
}
```

Output:

```text
[Hello ]
```

## See also

- [next_sibling](next_sibling.md)
- [sgcl::txt::html_node](README.md)
