[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::type

```cpp
kind type() const noexcept;
```

What the node is: an element, a text, a comment, an instruction, or `kind::none` for `xml()`
([kind](../xml-kind.md)).

## Parameters

None.

## Return value

The kind of the node.

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
    encoding::xml::options o;
    o.keep_comments = true;
    auto doc = encoding::xml::parse("<a>text<!--note--><b/><?go?></a>", o).value();
    for (auto& node : doc.children()) {
        switch (node.type()) {
            case encoding::xml::kind::element: println("element {}", node.name()); break;
            case encoding::xml::kind::text: println("text {}", node.text()); break;
            case encoding::xml::kind::comment: println("comment {}", node.text()); break;
            case encoding::xml::kind::instruction: println("instruction {}", node.name()); break;
            case encoding::xml::kind::none: break;
        }
    }
    println(doc.child("c").type() == encoding::xml::kind::none);
}
```

Output:

```text
text text
comment note
element b
instruction go
true
```

## See also

- [exists](exists.md), [is_element](is_element.md), [is_text](is_text.md): the questions asked most
- [kind](../xml-kind.md)
- [sgcl::encoding::xml](../xml.md)
