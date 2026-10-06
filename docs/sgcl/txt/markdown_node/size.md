[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::size

```cpp
size_t size() const noexcept;
```

Returns the number of children.

## Parameters

None.

## Return value

The number.

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
    println("{} {}", doc.root().size(), doc.root()[1].size());
}
```

Output:

```text
5 7
```

## See also

- [operator[]](operator_at.md)
- [sgcl::txt::markdown_node](README.md)
