[sgcl](../../README.md) › [txt](../README.md) › [markdown_node](README.md)

# sgcl::txt::markdown_node::checked

```cpp
optional<bool> checked() const noexcept;
```

Returns a task list item's box: `[x]` checked, `[ ]` not.

## Parameters

None.

## Return value

The box; nothing for an item that is no task and for other nodes.

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
    auto first = doc.root()[2][0].checked(), second = doc.root()[2][1].checked();
    println("{} {}", first.has_value(), second.value());
}
```

Output:

```text
false true
```

## See also

- [markdown_options](../markdown_options.md)
- [sgcl::txt::markdown_node](README.md)
