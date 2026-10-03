[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](README.md)

# sgcl::txt::fold_searcher::size

```cpp
size_t size() const noexcept;
```

Returns the number of code points searched for, which is not the length of the pattern in characters: `"ß"` folds
to two, and an `"é"` of one code point decomposes into two.

## Parameters

None.

## Return value

The number of code points the pattern mapped to.

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
    println("{} {}", txt::fold_searcher("ß").size(), txt::normalized_searcher("ß").size());
    println("{} {}", txt::fold_searcher("é").size(), txt::normalized_searcher("é").size());
}
```

Output:

```text
2 1
1 2
```

## See also

- [empty](empty.md): whether the pattern mapped to nothing
- [points](points.md): the code points
- [sgcl::txt::fold_searcher, normalized_searcher](README.md)
