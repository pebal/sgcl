[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::operator==

```cpp
friend bool operator==(const html_node& a, const html_node& b) noexcept;
```

Checks whether two handles hold the same node (not two equal ones).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the nodes |

## Return value

`true` for one node.

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
    auto a = doc.body().elements("a")[0];
    println("{} {}", a.parent() == doc.root().element_by_id("lead"),
            a == doc.body().elements("a")[1]);
}
```

Output:

```text
true false
```

## See also

- [parent](parent.md)
- [sgcl::txt::html_node](README.md)
