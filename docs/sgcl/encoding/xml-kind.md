[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md)

# sgcl::encoding::xml::kind

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        enum class kind : uint8_t { none, element, text, comment, instruction };
    };
}
```

`sgcl::encoding::xml::kind` is what a node of a tree is, as [type](xml/type.md) tells it. The XML declaration and
the DOCTYPE are no nodes: a tree does not hold them.

| Value | Description |
|---|---|
| `none` | no node: `xml()`, what [child](xml/child.md) gives when there is no such child |
| `element` | an element, with its attributes and children |
| `text` | a text: the pieces of one text, around references, CDATA sections and comments left out, are one node |
| `comment` | a comment, kept in a tree read with `options::keep_comments` |
| `instruction` | a processing instruction: its target is the node's name, its data the node's text |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

const char* name_of(encoding::xml::kind k) {
    using enum encoding::xml::kind;
    switch (k) {
        case none: return "none";
        case element: return "element";
        case text: return "text";
        case comment: return "comment";
        case instruction: return "instruction";
    }
    return "";
}

int main() {
    encoding::xml::options o;
    o.keep_comments = true;
    auto doc = encoding::xml::parse("<a>t<!--c--><b/><?p?></a>", o).value();
    for (auto& node : doc.children()) {
        print("{} ", name_of(node.type()));
    }
    println(name_of(doc.child("z").type()));
}
```

Output:

```text
text comment element instruction none
```

## See also

- [type](xml/type.md): the kind of a node
- [token::kind](xml-token-kind.md): the kinds of the tokens a reader gives
- [sgcl::encoding::xml](xml.md)
