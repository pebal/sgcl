[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches.md)

# sgcl::txt::fold_matches::empty

```cpp
bool empty() const noexcept;
```

Checks whether the pattern does not occur in the text, `begin() == end()`: one scan, which stops at the first
occurrence. An empty pattern occurs nowhere, so its range is empty.

## Parameters

None.

## Return value

`true` when there is no occurrence, the pattern is empty or the range was made by the default constructor; `false`
otherwise.

## Complexity

A scan of the mapped text to the first occurrence: linear on ordinary text, the text times the pattern at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::fold_matches("Kot", "KOT").empty(),
            txt::fold_matches("Kot", "pies").empty(), txt::fold_matches("Kot", "").empty());
}
```

Output:

```text
false true true
```

## See also

- [count](count.md): the number of occurrences
- [sgcl::txt::fold_matches, normalized_matches](../fold_matches.md)
