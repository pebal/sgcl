[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::title

```cpp
string title() const noexcept;
```

Returns a link's or an image's title.

## Parameters

None.

## Return value

The title; empty without one and for other nodes.

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
    println("{}", doc.root()[1][5].title());
}
```

Output:

```text
A
```

## See also

- [url](url.md)
- [sgcl::txt::markdown_node](README.md)
