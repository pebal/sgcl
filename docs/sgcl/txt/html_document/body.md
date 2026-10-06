[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::body

```cpp
html_node body() const noexcept;
```

Returns the `body` element (or the `frameset` of a frameset document).

## Parameters

None.

## Return value

The element; no node when there is none.

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
    println("{}", doc.body().name());
}
```

Output:

```text
body
```

## See also

- [root](root.md)
- [html](html.md)
- [sgcl::txt::html_document](README.md)
