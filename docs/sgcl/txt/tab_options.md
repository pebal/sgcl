[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::tab_options

```cpp
#include "sgcl/txt/tab_writer.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct tab_options {
        size_t min_width = 0;
        size_t tab_width = 8;
        size_t padding = 1;
        char pad_char = ' ';
        bool align_right = false;
        bool discard_empty_columns = false;
        bool tab_indent = false;
        bool filter_html = false;
        bool strip_escape = false;
        bool debug = false;
    };
}
```

`sgcl::txt::tab_options` is how [align_tabs](align_tabs.md) and [tab_writer](tab_writer/README.md) set their
columns: Go's `tabwriter.NewWriter` arguments and flags.

## Member objects

| Field | Description |
|---|---|
| `min_width` | the least width of a column, padding included; 0 by default |
| `tab_width` | the width of a tab, when the padding is tabs; 8 by default (0: tabs pad nothing) |
| `padding` | added to the widest cell of a column; 1 by default |
| `pad_char` | the character the padding is made of; a space by default. A tab (`'\t'`) pads with tabs, the columns at tab stops, and aligns every cell left |
| `align_right` | cells aligned right, the padding before them (Go's `AlignRight`) |
| `discard_empty_columns` | a column of empty cells all ended by soft tabs (`\v`) takes no room (`DiscardEmptyColumns`) |
| `tab_indent` | the leading empty cells of a line padded with tabs whatever `pad_char` is (`TabIndent`) |
| `filter_html` | a tag takes no width and an entity (`&amp;`) one column, both written as they are (`FilterHTML`) |
| `strip_escape` | the 0xFF bytes around an escaped piece dropped from the output (`StripEscape`) |
| `debug` | a `|` between the cells of a line, and `---` where a form feed ended the columns (`Debug`) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string table = "a\tb\tc\nddd\teeee\tf\n";
    print("{}", txt::align_tabs(table, {.min_width = 6}));
    print("{}", txt::align_tabs(table, {.debug = true}));
    print("{}", txt::align_tabs("<b>bold</b>\tx\nplain\ty\n", {.filter_html = true}));
}
```

Output:

```text
a     b     c
ddd   eeee  f
a   |b    |c
ddd |eeee |f
<b>bold</b>  x
plain y
```

## See also

- [align_tabs](align_tabs.md)
- [tab_writer](tab_writer/README.md)
- [sgcl::txt](README.md)
