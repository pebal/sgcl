[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::info

```cpp
string info() const noexcept;
```

Returns a fenced code block's info string, whose first word is the language.

## Parameters

None.

## Return value

The info string; empty for an indented block and for other nodes.

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
    println("{}", doc.root()[3].info());
}
```

Output:

```text
cpp
```

## See also

- [literal](literal.md)
- [sgcl::txt::markdown_node](README.md)
