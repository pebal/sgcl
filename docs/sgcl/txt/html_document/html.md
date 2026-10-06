[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::html

```cpp
html_node html() const noexcept;
```

Returns the `html` element (for a fragment, the root).

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
    println("{}", doc.html().name());
}
```

Output:

```text
html
```

## See also

- [root](root.md)
- [body](body.md)
- [sgcl::txt::html_document](README.md)
