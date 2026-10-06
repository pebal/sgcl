[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::ns

```cpp
html_namespace ns() const noexcept;
```

Returns an element's namespace.

## Parameters

None.

## Return value

An [html_namespace](../html_namespace.md): html, svg, mathml.

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
    auto svg = doc.body().elements("svg")[0];
    println("{} {}", int(svg.ns()), int(svg[0].ns()));
}
```

Output:

```text
1 1
```

## See also

- [html_namespace](../html_namespace.md)
- [sgcl::txt::html_node](README.md)
