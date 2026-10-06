[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::ordered

```cpp
bool ordered() const noexcept;
```

Checks whether a list is ordered (`1.`, `1)`).

## Parameters

None.

## Return value

`true` for an ordered list.

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
    println("{}", doc.root()[2].ordered());
}
```

Output:

```text
true
```

## See also

- [start](start.md)
- [tight](tight.md)
- [sgcl::txt::markdown_node](README.md)
