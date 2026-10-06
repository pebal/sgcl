[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::content

```cpp
html_node content() const noexcept;
```

Returns a `template` element's contents: a node of kind fragment, whose children are what the template holds (the
template itself has no children).

## Parameters

None.

## Return value

The fragment; no node for any other node.

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
    auto t = doc.body().elements("template")[0];
    println("{} {} {}", t.size(), int(t.content().kind()), t.content()[0].name());
}
```

Output:

```text
0 5 b
```

## See also

- [inner_html](inner_html.md)
- [sgcl::txt::html_node](README.md)
