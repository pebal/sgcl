[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](README.md)

# sgcl::txt::fold_matches::pattern

```cpp
const searcher_type& pattern() const noexcept;
```

Returns the pattern the range looks for, mapped: a [fold_searcher](../fold_searcher/README.md) for `fold_matches`, a
[normalized_searcher](../fold_searcher/README.md) for `normalized_matches`. Its [pattern](../fold_searcher/pattern.md) is
the text as it was given.

## Parameters

None.

## Return value

The searcher; one of an empty pattern for a range made by the default constructor.

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
    txt::fold_matches matches("Fuß und FUSS", "Fuß");
    println("{}: {} code points, {} matches", matches.pattern().pattern(), matches.pattern().size(),
            matches.count());
}
```

Output:

```text
Fuß: 4 code points, 2 matches
```

## See also

- [text](text.md): the text
- [sgcl::txt::fold_matches, normalized_matches](README.md)
