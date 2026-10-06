[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::inner_html

```cpp
string inner_html() const;
```

Returns the HTML fragment serialization algorithm (13.3) of the node's children (of a template, of its contents).

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
    println("{}", doc.root().element_by_id("lead").inner_html());
}
```

Output:

```text
Hello <a href="/a">first</a> and <a href="https://x.org/b">second</a>. <template><b>later</b></template><svg viewBox="0 0 9 9"><circle r="4"></circle></svg>
```

## See also

- [outer_html](outer_html.md)
- [html_document::to_string](../html_document/to_string.md)
- [sgcl::txt::html_node](README.md)
