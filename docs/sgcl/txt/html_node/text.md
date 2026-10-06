[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::text

```cpp
string text() const;
```

Returns textContent: the text of every descendant text node, in document order (a template's contents not counted).

## Parameters

None.

## Return value

The text.

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
    println("[{}]", doc.root().element_by_id("lead").text());
}
```

Output:

```text
[Hello first and second. ]
```

## See also

- [data](data.md)
- [sgcl::txt::html_node](README.md)
