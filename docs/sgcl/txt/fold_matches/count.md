[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](README.md)

# sgcl::txt::fold_matches::count

```cpp
size_type count() const noexcept;
```

Returns the number of occurrences, which do not overlap: walked and counted, not stored, over the text mapped once.
It is the count [folded_text::count](../folded_text/count.md) gives of the same text and pattern.

## Parameters

None.

## Return value

The number of occurrences; 0 for an empty pattern and a range made by the default constructor.

## Complexity

A scan of the mapped text: linear on ordinary text, the text times the pattern at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::fold_matches("ΣΊΣΥΦΟΣ, σίσυφος, Σίσυφος", "σίσυφος").count());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md): whether there is no occurrence
- [sgcl::txt::fold_matches, normalized_matches](README.md)
