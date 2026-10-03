[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [reader](../csv-reader.md)

# sgcl::encoding::csv::reader::line

```cpp
uint32_t line() const noexcept;
```

The line the reader has reached, from 1: where the next record starts, unless empty lines or comments come before
it, which are counted only as the next record is read. After a record, the line after its last; at the start, 1.

## Parameters

None.

## Return value

The line.

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
    encoding::csv::reader r("a\n\"b\nc\"\n\n\nd\n");
    println("{}", r.line());
    while (auto row = r.next()) {
        println("{} on line {}, the reader at {}", row->at(0).size(), row->line(), r.line());
    }
}
```

Output:

```text
1
1 on line 1, the reader at 2
3 on line 2, the reader at 4
1 on line 6, the reader at 7
```

## See also

- [row::line](../csv-row/line.md): the line a record starts on
- [sgcl::encoding::csv::reader](../csv-reader.md)
