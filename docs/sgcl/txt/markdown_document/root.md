[sgcl](../../README.md) › [txt](../README.md) › [markdown_document](README.md)

# sgcl::txt::markdown_document::root

```cpp
markdown_node root() const noexcept;
```

Returns the document node, whose children are the blocks.

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
    auto doc = txt::markdown_document::parse(
        "# Notes\n\nSome *text* and `code`, a [link](/a \"A\").\n\n"
        "3. three\n4. [x] four\n\n~~~cpp\nint x;\n~~~\n\n| a | b |\n|:-|-:|\n| 1 | 2 |\n");
    for (size_t k = 0; k < doc.root().size(); ++k) {
        println("{}", int(doc.root()[k].kind()));
    }
}
```

Output:

```text
7
6
2
4
9
```

## See also

- [markdown_node](../markdown_node/README.md)
- [sgcl::txt::markdown_document](README.md)
