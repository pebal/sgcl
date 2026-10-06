[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a node.

## Parameters

None.

## Return value

`true` for a node.

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
    println("{} {}", bool(doc.body()), bool(doc.body().parent().parent().parent()));
}
```

Output:

```text
true false
```

## See also

- [(constructor)](html_node.md)
- [sgcl::txt::html_node](README.md)
