[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::kind

```cpp
html_node_kind kind() const noexcept;
```

Returns what the node is.

## Parameters

None.

## Return value

An [html_node_kind](../html_node_kind.md): document, doctype, element, text, comment, fragment.

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
    auto p = doc.root().element_by_id("lead");
    for (size_t k = 0; k < p.size(); ++k) {
        println("{}", int(p[k].kind()));
    }
}
```

Output:

```text
3
2
3
2
3
2
2
```

## See also

- [html_node_kind](../html_node_kind.md)
- [sgcl::txt::html_node](README.md)
