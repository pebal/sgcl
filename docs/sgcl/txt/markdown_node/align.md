[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::align

```cpp
markdown_align align() const noexcept;
```

Returns a table cell's alignment.

## Parameters

None.

## Return value

The [markdown_align](../markdown_align.md); `none` for other nodes.

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
    println("{} {}", int(doc.root()[4][1][0].align()), int(doc.root()[4][1][1].align()));
}
```

Output:

```text
1 3
```

## See also

- [header](header.md)
- [sgcl::txt::markdown_node](README.md)
