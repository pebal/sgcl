[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](../fold_searcher.md)

# sgcl::txt::fold_searcher::empty

```cpp
bool empty() const noexcept;
```

Checks whether the pattern mapped to no code points, `size() == 0`: only an empty pattern does. An empty pattern is
found at once by [find](find.md) and counted nowhere by [count](count.md) and the ranges.

## Parameters

None.

## Return value

`true` when the pattern mapped to nothing, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::fold_searcher none("");
    println("{} {} {}", none.empty(), none.find("abc", 2)->pos, none.count("abc"));
}
```

Output:

```text
true 2 0
```

## See also

- [size](size.md): the number of code points
- [sgcl::txt::fold_searcher, normalized_searcher](../fold_searcher.md)
