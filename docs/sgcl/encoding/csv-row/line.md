[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [row](../csv-row.md)

# sgcl::encoding::csv::row::line

```cpp
uint32_t line() const noexcept;
```

The line the record starts on, from 1. A quoted field may hold line endings, so a record may span several lines, and
the empty lines and the comments before it are counted: the line is the text's, as an editor shows it.

## Parameters

None.

## Return value

The line, from 1; 0 for a row with no fields.

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
    encoding::csv::reader r("a,\"one\ntwo\"\n\n# skipped\nb,c\n", {.comment = '#'});
    while (auto row = r.next()) {
        println("{} starts on line {}", row->at(0), row->line());
    }
}
```

Output:

```text
a starts on line 1
b starts on line 5
```

## See also

- [position](position.md): the line and the column of a field
- [reader::line](../csv-reader/line.md): the line the reader is at
- [sgcl::encoding::csv::row](../csv-row.md)
