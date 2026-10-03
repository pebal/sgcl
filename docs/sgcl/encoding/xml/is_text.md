[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::is_text

```cpp
bool is_text() const noexcept;
```

Checks whether the node is a text: `type() == kind::text`. In a tree read by [parse](parse.md), the pieces of one
text — around a reference, a CDATA section, a comment left out — are one text node.

## Parameters

None.

## Return value

`true` for a text; `false` for an element, a comment, an instruction and `xml()`.

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
    auto p = encoding::xml::parse("<p>Fish &amp; <![CDATA[<chips>]]><br/>to go</p>").value();
    for (auto& node : p.children()) {
        if (node.is_text()) {
            println("[{}]", node.text());
        }
    }
}
```

Output:

```text
[Fish & <chips>]
[to go]
```

## See also

- [is_element](is_element.md), [type](type.md)
- [text](text.md): the text of a node
- [sgcl::encoding::xml](../xml.md)
