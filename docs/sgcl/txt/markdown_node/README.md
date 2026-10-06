[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::markdown_node

```cpp
#include "sgcl/txt/markdown.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class markdown_node;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::markdown_node` is a node of a parsed [markdown_document](../markdown_document/README.md): a block — a
paragraph, a heading, a list and its items, a code block, a table — or an inline — text, emphasis, a code span, a
link — with its parent, its children and what its kind holds. A handle to an immutable node.

## Rules

- A handle of one word, shared by copies; equal when it is the same node.
- A node keeps its document alive.
- Walking and writing a tree of any depth uses no recursion.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](markdown_node.md) | constructs no node |

#### What it is

| Function | Description |
|---|---|
| [kind](kind.md) | what the node is |
| [operator bool](operator_bool.md) | checks whether it holds a node |
| [operator==](operator_cmp.md) | checks whether two handles hold one node |

#### The tree

| Function | Description |
|---|---|
| [parent](parent.md) | the parent |
| [size](size.md) | the number of children |
| [operator[]](operator_at.md) | a child by its index |

#### What it holds

| Function | Description |
|---|---|
| [literal](literal.md) | a text's, a code span's, a code block's, raw HTML's text |
| [url](url.md) | a link's or an image's destination |
| [title](title.md) | its title |
| [info](info.md) | a code block's info string |
| [level](level.md) | a heading's level |
| [ordered](ordered.md) | whether a list is ordered |
| [start](start.md) | an ordered list's first number |
| [tight](tight.md) | whether a list is tight |
| [checked](checked.md) | a task item's box |
| [align](align.md) | a table cell's alignment |
| [header](header.md) | whether a row or cell is the header's |

#### Text and HTML

| Function | Description |
|---|---|
| [text](text.md) | the text of the node and its descendants |
| [to_html](to_html.md) | the HTML of the node and its descendants |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::markdown_document::parse(
        "See [one](/1), *[two](/2 \"Two\")* and <https://three.org>.\n");
    // the links of a document
    vector<txt::markdown_node> stack;
    stack.push_back(doc.root());
    while (!stack.empty()) {
        txt::markdown_node n = stack.back();
        stack.pop_back();
        if (n.kind() == txt::markdown_kind::link) {
            println("{} -> {}", n.text(), n.url());
        }
        for (size_t k = n.size(); k-- > 0;) {
            stack.push_back(n[k]);
        }
    }
}
```

Output:

```text
one -> /1
two -> /2
https://three.org -> https://three.org
```

## See also

- [markdown_document](../markdown_document/README.md)
- [markdown_kind](../markdown_kind.md)
- [sgcl::txt](../README.md)
