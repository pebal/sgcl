[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::data

```cpp
string data() const noexcept;
```

Returns a text node's or a comment's text, or a DOCTYPE's name.

## Parameters

None.

## Return value

The text; empty for elements.

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
    println("[{}]", p.first_child().data());
}
```

Output:

```text
[Hello ]
```

## See also

- [text](text.md)
- [sgcl::txt::html_node](README.md)
