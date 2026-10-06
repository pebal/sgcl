[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::name

```cpp
string name() const noexcept;
```

Returns an element's local name: lowercase for HTML, SVG's own case (`foreignObject`, `clipPath`) and MathML's.

## Parameters

None.

## Return value

The name; empty for a node that is not an element.

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
    for (const auto &e : doc.body().elements()) {
        print("{} ", e.name());
    }
    println("");
}
```

Output:

```text
p a a template svg circle 
```

## See also

- [ns](ns.md)
- [sgcl::txt::html_node](README.md)
