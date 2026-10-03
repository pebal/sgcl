[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::level

```cpp
strength level() const noexcept;
```

Returns how much of a difference counts to the collator: the [strength](../strength.md) it was made with,
`tertiary` unless one was given.

## Parameters

None.

## Return value

The strength.

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
    txt::collator plain;
    txt::collator search{txt::strength::primary};
    println("{} {}", plain.level() == txt::strength::tertiary,
            search.level() == txt::strength::primary);
}
```

Output:

```text
true true
```

## See also

- [strength](../strength.md)
- [sgcl::txt::collator](../collator.md)
