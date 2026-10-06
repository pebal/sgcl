[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::level

```cpp
int level() const noexcept;
```

Returns a heading's level.

## Parameters

None.

## Return value

1 to 6 for a heading; 0 for other nodes.

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
    println("{} {}", doc.root()[0].level(), doc.root()[1].level());
}
```

Output:

```text
1 0
```

## See also

- [kind](kind.md)
- [sgcl::txt::markdown_node](README.md)
