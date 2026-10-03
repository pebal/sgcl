[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](../collated_matches.md)

# sgcl::txt::collated_matches::empty

```cpp
bool empty() const noexcept;
```

Checks whether the pattern is not found in the text, `begin() == end()`: one scan, which stops at the first
occurrence. An empty pattern, and one of which the collator looks at nothing, occur nowhere, so their range is
empty.

## Parameters

None.

## Return value

`true` when there is no occurrence, the pattern is empty or the range was made by the default constructor; `false`
otherwise.

## Complexity

A scan of the weighed text to the first occurrence: linear on ordinary text, the text times the pattern at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator secondary{txt::strength::secondary};
    println("{} {}", txt::collated_matches(secondary, "RÉSUMÉ", "résumé").empty(),
            txt::collated_matches(secondary, "RESUME", "résumé").empty());
}
```

Output:

```text
false true
```

## See also

- [count](count.md): the number of occurrences
- [sgcl::txt::collated_matches](../collated_matches.md)
