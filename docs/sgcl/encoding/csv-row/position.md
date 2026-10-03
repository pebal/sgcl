[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::position

```cpp
pair<uint32_t, uint32_t> position(size_t index) const noexcept;
```

The line and the column where the field at `index` starts: its first character, or its opening quote when it is
quoted, as Go's `FieldPos` gives them. The column counts code points from 1, where Go counts bytes: `ż,x` has `x`
in column 3 here, 4 in Go; the two are the same for ASCII. With
[options](../csv-options.md)`::trim_leading_space`, the place is past the spaces dropped.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the field's index, from 0 |

## Return value

The line and the column, from 1; `{0, 0}` when `index` is not below [size()](size.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("żółw,x,\"two\nlines\",y\n");
    auto row = r.next().value();
    for (int i : range(5)) {
        auto [line, column] = row.position(i);
        println("{}: {}:{}", i, line, column);
    }
}
```

Output:

```text
0: 1:1
1: 1:6
2: 1:8
3: 2:8
4: 0:0
```

## See also

- [line](line.md): the line the record starts on
- [error::line](../error/line.md), [error::column](../error/column.md): the place of a mistake
- [sgcl::encoding::csv::row](README.md)
