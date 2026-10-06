[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::markdown_document

```cpp
#include "sgcl/txt/markdown.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class markdown_document;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::markdown_document` is a Markdown text read into a tree — CommonMark 0.31.2 and GitHub's extensions — of
immutable [markdown_node](../markdown_node/README.md)s, to walk (the headings of a page, its links, its code) or to
write as HTML. [markdown_to_html](../markdown_to_html.md) is the same in one step, without the tree.

## Rules

- A handle of one word, shared by copies; the tree does not change once read.
- Nothing recurses: a document nested as deep as its text says is read, walked and written.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](markdown_document.md) | constructs no document |
| [parse](parse.md) | reads a text |

#### Observers

| Function | Description |
|---|---|
| [root](root.md) | the document node |
| [to_html](to_html.md) | the HTML |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto doc = txt::markdown_document::parse("# One\n\ntext\n\n## Two\n\n### Three\n");
    for (size_t k = 0; k < doc.root().size(); ++k) {
        txt::markdown_node block = doc.root()[k];
        if (block.kind() == txt::markdown_kind::heading) {
            println("{}{}", string(size_t(block.level() - 1) * 2, ' '), block.text());
        }
    }
}
```

Output:

```text
One
  Two
    Three
```

## See also

- [markdown_node](../markdown_node/README.md)
- [markdown_to_html](../markdown_to_html.md)
- [sgcl::txt](../README.md)
