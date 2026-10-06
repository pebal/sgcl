[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::element_by_id

```cpp
html_node element_by_id(const string& id) const noexcept;
```

Returns the first descendant element, in document order, whose `id` is `id`.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the id |

## Return value

The element; no node without one.

## Complexity

Linear in the size of the subtree.

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
    println("{} {}", doc.root().element_by_id("lead").name(), bool(doc.root().element_by_id("x")));
}
```

Output:

```text
p false
```

## See also

- [elements](elements.md)
- [sgcl::txt::html_node](README.md)
