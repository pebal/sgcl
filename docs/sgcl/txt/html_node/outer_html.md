[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::outer_html

```cpp
string outer_html() const;
```

Returns the node itself serialized, its start tag, children and end tag.

## Parameters

None.

## Return value

The HTML.

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
    println("{}", doc.body().elements("svg")[0].outer_html());
}
```

Output:

```text
<svg viewBox="0 0 9 9"><circle r="4"></circle></svg>
```

## See also

- [inner_html](inner_html.md)
- [sgcl::txt::html_node](README.md)
