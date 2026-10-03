[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::row

```cpp
row() noexcept = default;
```

An empty row: no fields, no header, line 0. The rows with fields are made by a [reader](../csv-reader/README.md); this one
is for a variable that gets a row later, and copies and assignments are the implicit ones.

## Parameters

None.

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
    encoding::csv::row last;
    println("{} {} {}", last.empty(), last.line(), last["name"].has_value());

    encoding::csv::reader r("a,b\nc,d\n");
    while (auto row = r.next()) {
        last = *row;
    }
    println("{} {} {}", last.empty(), last.line(), last[0]);
}
```

Output:

```text
true 0 false
false 2 c
```

## See also

- [reader::next](../csv-reader/next.md): a row read
- [sgcl::encoding::csv::row](README.md)
