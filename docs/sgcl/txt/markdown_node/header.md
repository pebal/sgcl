[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::header

```cpp
bool header() const noexcept;
```

Checks whether a table row or cell is the header's.

## Parameters

None.

## Return value

`true` for the first row of a table and its cells.

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
    println("{} {}", doc.root()[4][0].header(), doc.root()[4][1].header());
}
```

Output:

```text
true false
```

## See also

- [align](align.md)
- [sgcl::txt::markdown_node](README.md)
