[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](README.md)

# sgcl::txt::fold_searcher::pattern

```cpp
const string& pattern() const noexcept;
```

Returns the pattern as it was given, before it was folded or decomposed: the searcher keeps it so that it can say
what it looks for, as [searcher](../searcher/README.md) does.

## Parameters

None.

## Return value

The pattern.

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
    txt::fold_searcher s("Größe");
    println("{}, {} code points", s.pattern(), s.size());
}
```

Output:

```text
Größe, 6 code points
```

## See also

- [points](points.md): the pattern as it is searched for
- [sgcl::txt::fold_searcher, normalized_searcher](README.md)
