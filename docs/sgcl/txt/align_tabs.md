[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::align_tabs

```cpp
#include "sgcl/txt/tab_writer.h"   // or "sgcl/txt.h"

string align_tabs(const string& text, const tab_options& o = {});
```

Returns the text with its cells aligned in columns, as Go's `text/tabwriter` aligns them (elastic tabstops). Cells
end at a tab (`\t`) or a soft tab (`\v`), lines at a line feed (`\n`) or a form feed (`\f`). The tab-ended cells at
one place of consecutive lines are a column, as wide as its widest cell and the padding; the last cell of a line,
ended by its line break, is in no column. A line without a tab ends the columns above it, and a form feed ends every
column (it is written as a line feed). Text between two 0xFF bytes is one piece of a cell, its tabs and line breaks
included. A cell is as wide as a terminal shows it — an East Asian wide character two columns, a combining mark none
— where Go counts code points; for text of one column a character the output is Go's.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the cells, their tabs and line breaks |
| `o` | the [tab_options](tab_options.md): widths, padding, alignment, HTML, escapes |

## Return value

The aligned text.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    print("{}", txt::align_tabs("name\tsize\tkind\nmain.cpp\t1204\tsource\nREADME\t88\ttext\n"));
    print("{}", txt::align_tabs("名前\tサイズ\nファイル\t12\n", {.padding = 2}));
    print("{}", txt::align_tabs("a\tbb\tc\nddd\te\tf\n", {.pad_char = '.', .align_right = true}));
}
```

Output:

```text
name     size kind
main.cpp 1204 source
README   88   text
名前      サイズ
ファイル  12
...a.bbc
.ddd..ef
```

## See also

- [tab_writer](tab_writer/README.md)
- [tab_options](tab_options.md)
- [columns](columns.md)
- [sgcl::txt](README.md)
