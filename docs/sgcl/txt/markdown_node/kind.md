[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::kind

```cpp
markdown_kind kind() const noexcept;
```

Returns what the node is.

## Parameters

None.

## Return value

The [markdown_kind](../markdown_kind.md); `document` for no node.

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
    auto doc = txt::markdown_document::parse(
        "# Notes\n\nSome *text* and `code`, a [link](/a \"A\").\n\n"
        "3. three\n4. [x] four\n\n~~~cpp\nint x;\n~~~\n\n| a | b |\n|:-|-:|\n| 1 | 2 |\n");
    println("{} {}", int(doc.root()[0].kind()), int(doc.root()[1].kind()));
}
```

Output:

```text
7 6
```

## See also

- [markdown_kind](../markdown_kind.md)
- [sgcl::txt::markdown_node](README.md)
