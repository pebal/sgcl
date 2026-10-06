[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::markdown_kind

```cpp
#include "sgcl/txt/markdown.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class markdown_kind : uint8_t {
        document,
        block_quote,
        list,
        item,
        code_block,
        html_block,
        paragraph,
        heading,
        thematic_break,
        table,
        table_row,
        table_cell,
        text,
        soft_break,
        line_break,
        code,
        html_inline,
        emphasis,
        strong,
        strikethrough,
        link,
        image,
    };
}
```

What a [markdown_node](markdown_node/README.md) is: a block or an inline.

| Value | Description |
|---|---|
| `document` | the root |
| `block_quote` | a block quote, `>` |
| `list` | a list of items: ordered or not, tight or loose |
| `item` | an item of a list |
| `code_block` | an indented or fenced code block |
| `html_block` | a block of raw HTML |
| `paragraph` | a paragraph |
| `heading` | an ATX or setext heading, of a level |
| `thematic_break` | a horizontal rule |
| `table` | a table (GitHub) |
| `table_row` | a row of a table, the header's first |
| `table_cell` | a cell of a row |
| `text` | text |
| `soft_break` | a line break within a paragraph |
| `line_break` | a hard line break: two spaces or a backslash before it |
| `code` | a code span |
| `html_inline` | raw HTML within text |
| `emphasis` | `*` or `_` |
| `strong` | `**` or `__` |
| `strikethrough` | `~` or `~~` (GitHub) |
| `link` | a link |
| `image` | an image |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto root = txt::markdown_document::parse("# A\n\n> b *c*\n").root();
    println("{} {} {}", root[0].kind() == txt::markdown_kind::heading,
            root[1].kind() == txt::markdown_kind::block_quote,
            root[1][0][1].kind() == txt::markdown_kind::emphasis);
}
```

Output:

```text
true true true
```

## See also

- [markdown_node::kind](markdown_node/kind.md)
- [sgcl::txt](README.md)
