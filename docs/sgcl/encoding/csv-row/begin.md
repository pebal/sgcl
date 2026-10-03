[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::begin

```cpp
iterator begin() const noexcept;
```

An iterator to the first field, for a range-for over the fields: `for (auto field : row)`. The iterator is a forward
one; dereferenced, it gives the field as a `slice<const char>`, by value. It refers to the row, which outlives it.

## Parameters

None.

## Return value

An iterator to the first field, equal to [end()](end.md) for a row with no fields.

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
    encoding::csv::reader r("one,\"two, three\",four\n");
    auto row = r.next().value();
    for (auto field : row) {
        println("[{}]", field);
    }
    println("{}", *row.begin());
}
```

Output:

```text
[one]
[two, three]
[four]
one
```

## See also

- [end](end.md): the iterator past the last field
- [sgcl::encoding::csv::row](README.md)
