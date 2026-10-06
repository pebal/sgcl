[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::root

```cpp
html_node root() const noexcept;
```

Returns the document node, whose children are the DOCTYPE, the comments outside the `html` element and the `html`
element; for a fragment, the `html` element holding it.

## Parameters

None.

## Return value

The node; no node for no document.

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
    for (size_t k = 0; k < doc.root().size(); ++k) {
        println("{}", doc.root()[k].kind() == txt::html_node_kind::doctype ? "doctype"
                                                                           : doc.root()[k].name());
    }
}
```

Output:

```text
doctype
html
```

## See also

- [html](html.md)
- [html_node](../html_node/README.md)
- [sgcl::txt::html_document](README.md)
