[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::size

```cpp
size_t size() const noexcept;
```

The number of fields of the record. With [options](../csv-options.md)`::same_field_count`, the default, every row of
a reader has the size of the first; a record with nothing between two separators has an empty field there, and a
line of one empty quoted field (`""`) is one field.

## Parameters

None.

## Return value

The number of fields, 0 for a default-constructed row.

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
    encoding::csv::reader r("a,,c\n\"\"\n,\n", {.same_field_count = false});
    while (auto row = r.next()) {
        println("{}", row->size());
    }
}
```

Output:

```text
3
1
2
```

## See also

- [empty](empty.md): whether there are no fields
- [sgcl::encoding::csv::row](README.md)
