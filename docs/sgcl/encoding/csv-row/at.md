[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::at

```cpp
slice<const char> at(size_t index) const;
```

The field at `index`, checked: an index past the fields throws, where [operator\[\]](operator_at.md) asks the caller
to keep it below [size()](size.md).

## Parameters

| Parameter | Description |
|---|---|
| `index` | the field's index, from 0 |

## Return value

The field, as a slice of the row's own text.

## Complexity

Constant.

## Exceptions

`out_of_range` when `index` is not below `size()`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::csv::reader r("a,b\n");
    auto row = r.next().value();
    println("{}", row.at(1));
    try {
        row.at(2);
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
b
sgcl::encoding::csv::row::at
```

## See also

- [operator\[\]](operator_at.md): a field by its index, unchecked, or by the header's name
- [sgcl::encoding::csv::row](README.md)
