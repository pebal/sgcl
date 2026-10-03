[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [row](README.md)

# sgcl::encoding::csv::row::end

```cpp
iterator end() const noexcept;
```

The iterator past the last field: [begin()](begin.md) moved [size()](size.md) times.

## Parameters

None.

## Return value

The iterator past the last field.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <iterator>

using namespace sgcl;

int main() {
    encoding::csv::reader r("a,b,c\n");
    auto row = r.next().value();
    println("{}", std::distance(row.begin(), row.end()));
    println("{}", row.begin() == row.end());
}
```

Output:

```text
3
false
```

## See also

- [begin](begin.md): the iterator to the first field
- [sgcl::encoding::csv::row](README.md)
