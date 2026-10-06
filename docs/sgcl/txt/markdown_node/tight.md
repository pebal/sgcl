[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::tight

```cpp
bool tight() const noexcept;
```

Checks whether a list is tight: no blank line between its items or between the blocks of one, so that its paragraphs
are written without `<p>`.

## Parameters

None.

## Return value

`true` for a tight list (and for nodes that are no list).

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
    auto loose = txt::markdown_document::parse("- a\n\n- b\n").root()[0];
    println("{} {}", doc.root()[2].tight(), loose.tight());
}
```

Output:

```text
true false
```

## See also

- [ordered](ordered.md)
- [sgcl::txt::markdown_node](README.md)
