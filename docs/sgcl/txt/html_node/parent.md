[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::parent

```cpp
html_node parent() const noexcept;
```

Returns the node's parent.

## Parameters

None.

## Return value

The parent; no node for the document (and for a template's contents, the template).

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
    println("{} {}", a.parent().name(), a.parent().parent().name());
}
```

Output:

```text
p body
```

## See also

- [operator[]](operator_at.md)
- [sgcl::txt::html_node](README.md)
