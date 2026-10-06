[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::operator==

```cpp
bool operator==(const markdown_node& other) const noexcept;
```

Checks whether two handles hold the same node.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the other handle |

## Return value

`true` for one node.

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
    println("{} {}", doc.root()[0].parent() == doc.root(), doc.root()[0] == doc.root()[1]);
}
```

Output:

```text
true false
```

## See also

- [parent](parent.md)
- [sgcl::txt::markdown_node](README.md)
