[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::is_element

```cpp
bool is_element() const noexcept;
```

Checks whether the node is an element: `type() == kind::element`.

## Parameters

None.

## Return value

`true` for an element; `false` for a text, a comment, an instruction and `xml()`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = encoding::xml::parse("<p>Hello <b>you</b> and <i>them</i>!</p>").value();
    for (auto& node : p.children()) {
        if (node.is_element()) {
            println(node.name());
        }
    }
}
```

Output:

```text
b
i
```

## See also

- [is_text](is_text.md), [type](type.md)
- [children](children.md): the nodes inside an element
- [sgcl::encoding::xml](../xml.md)
