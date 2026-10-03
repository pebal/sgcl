[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::empty

```cpp
bool empty() const noexcept;
```

Checks whether the row has no fields. A row a reader returns has at least one, since an empty line is skipped and
not read as a record; an empty row is a default-constructed one.

## Parameters

None.

## Return value

`true` when [size()](size.md) is 0.

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
    encoding::csv::row none;
    println("{}", none.empty());

    encoding::csv::reader r("\n\n\"\"\n");
    auto row = r.next();
    println("{} {} {}", row->empty(), row->size(), row->line());
}
```

Output:

```text
true
false 1 3
```

## See also

- [size](size.md): the number of fields
- [sgcl::encoding::csv::row](README.md)
