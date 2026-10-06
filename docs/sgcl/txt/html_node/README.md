[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::html_node

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class html_node;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::html_node` is a node of a parsed [html_document](../html_document/README.md): the document, a DOCTYPE,
an element, a text, a comment or a template's contents, with its parent, its children and, for an element, its name,
namespace and attributes. A handle to an immutable node: the tree does not change once parsed.

## Rules

- A handle of one word, shared by copies; equal when it is the same node.
- A node keeps its document alive: a handle taken from a document outlives the document's own handle.
- Walking a tree of any depth uses no recursion ([elements](elements.md), [text](text.md), the serialization).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](html_node.md) | constructs no node |

#### What it is

| Function | Description |
|---|---|
| [kind](kind.md) | document, doctype, element, text, comment, fragment |
| [name](name.md) | an element's local name |
| [ns](ns.md) | an element's namespace |
| [attributes](attributes.md) | an element's attributes |
| [attribute](attribute.md) | an attribute's value |
| [data](data.md) | a text's or a comment's text |
| [operator bool](operator_bool.md) | checks whether it holds a node |
| [operator==](operator_cmp.md) | checks whether two handles hold one node |

#### The tree

| Function | Description |
|---|---|
| [parent](parent.md) | the parent |
| [size](size.md) | the number of children |
| [operator[]](operator_at.md) | a child by its index |
| [first_child](first_child.md) | the first child |
| [next_sibling](next_sibling.md) | the next and the previous sibling |
| [content](content.md) | a template's contents |
| [elements](elements.md) | the descendant elements, of a name or all |
| [element_by_id](element_by_id.md) | the element of an id |

#### Text and HTML

| Function | Description |
|---|---|
| [text](text.md) | the text of the descendants |
| [inner_html](inner_html.md) | the children serialized |
| [outer_html](outer_html.md) | the node serialized |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::html_document::parse(
        "<!DOCTYPE html><title>News</title><p id=lead>Hello "
        "<a href=\"/a\">first</a> and <a href=\"https://x.org/b\">second</a>. "
        "<template><b>later</b></template><svg viewbox=\"0 0 9 9\"><circle r=4></svg>");
    auto p = doc.root().element_by_id("lead");
    for (size_t k = 0; k < p.size(); ++k) {
        auto c = p[k];
        println("{}: {}", c.kind() == txt::html_node_kind::text ? "text" : c.name(),
                c.outer_html());
    }
}
```

Output:

```text
text: Hello 
a: <a href="/a">first</a>
text:  and 
a: <a href="https://x.org/b">second</a>
text: . 
template: <template><b>later</b></template>
svg: <svg viewBox="0 0 9 9"><circle r="4"></circle></svg>
```

## See also

- [html_document](../html_document/README.md)
- [html_attribute](../html_attribute.md)
- [sgcl::txt](../README.md)
