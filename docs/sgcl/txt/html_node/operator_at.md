[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::operator[]

```cpp
html_node operator[](size_t i) const noexcept;
```

Returns a child by its index.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the index, from 0 |

## Return value

The child; no node past the last.

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
    println("{} {} {}", p[1].name(), p[1][0].data(), bool(p[99]));
}
```

Output:

```text
a first false
```

## See also

- [size](size.md)
- [first_child](first_child.md)
- [sgcl::txt::html_node](README.md)
