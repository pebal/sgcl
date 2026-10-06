[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::parent

```cpp
markdown_node parent() const noexcept;
```

Returns the parent.

## Parameters

None.

## Return value

The parent; no node for the document node and for no node.

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
    println("{}", doc.root()[1][1].parent() == doc.root()[1]);
}
```

Output:

```text
true
```

## See also

- [operator[]](operator_at.md)
- [sgcl::txt::markdown_node](README.md)
