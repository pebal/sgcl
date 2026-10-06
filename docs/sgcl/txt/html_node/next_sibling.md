[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::next_sibling

```cpp
html_node next_sibling() const noexcept;        // (1)
html_node previous_sibling() const noexcept;    // (2)
```

Returns the node after (1) or before (2) this one in its parent.

## Parameters

None.

## Return value

The sibling; no node at the end.

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
    println("[{}] [{}]", a.next_sibling().data(), a.previous_sibling().data());
}
```

Output:

```text
[ and ] [Hello ]
```

## See also

- [first_child](first_child.md)
- [parent](parent.md)
- [sgcl::txt::html_node](README.md)
