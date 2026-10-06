[sgcl](../../README.md) › [txt](../README.md) › [tab_writer](README.md)

# sgcl::txt::tab_writer::write

```cpp
string write(const string& text);
```

Takes more text and gives back the lines whose columns it ended, aligned: a column's width is known only when its
last line is, so lines are held until a line without a tab or a form feed ends their block. Text may be cut
anywhere, inside a cell, a code point or an escape. Cells end at a tab (`\t`) or a soft tab (`\v`), lines at a line
feed (`\n`) or a form feed (`\f`). The tab-ended cells at one place of consecutive lines are a column, as wide as
its widest cell and the padding; the last cell of a line, ended by its line break, is in no column. A line without a
tab ends the columns above it, and a form feed ends every column (it is written as a line feed). Text between two
0xFF bytes is one piece of a cell, its tabs and line breaks included.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the next piece of the text |

## Return value

The lines the text completed, aligned; empty while the block of columns goes on.

## Complexity

Linear in the length of the text and of the lines it completes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::tab_writer w;
    println("[{}]", w.write("name\tsize\n"));
    println("[{}]", w.write("main.cpp\t1204\n"));
    print("{}", w.write("total 2 files\n"));
}
```

Output:

```text
[]
[]
name     size
main.cpp 1204
total 2 files
```

## See also

- [flush](flush.md)
- [align_tabs](../align_tabs.md)
- [sgcl::txt::tab_writer](README.md)
