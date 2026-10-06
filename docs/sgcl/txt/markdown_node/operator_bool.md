[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a node.

## Parameters

None.

## Return value

`true` for a node, `false` for none.

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
    println("{} {}", bool(doc.root()), bool(doc.root()[99]));
}
```

Output:

```text
true false
```

## See also

- [markdown_node](markdown_node.md)
- [sgcl::txt::markdown_node](README.md)
