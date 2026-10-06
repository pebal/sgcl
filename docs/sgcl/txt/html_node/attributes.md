[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::attributes

```cpp
slice<const html_attribute> attributes() const noexcept;
```

Returns an element's attributes in the order of its start tag, a name given twice kept once (the first).

## Parameters

None.

## Return value

The [html_attribute](../html_attribute.md)s; none for other nodes.

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
    for (const auto &a : doc.body().elements("svg")[0].attributes()) {
        println("{}={}", a.name, a.value);
    }
}
```

Output:

```text
viewBox=0 0 9 9
```

## See also

- [attribute](attribute.md)
- [html_attribute](../html_attribute.md)
- [sgcl::txt::html_node](README.md)
