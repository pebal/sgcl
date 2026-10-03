[sgcl](../../README.md) › [txt](../README.md) › [collated_searcher](../collated_searcher.md)

# sgcl::txt::collated_searcher::empty

```cpp
bool empty() const noexcept;
```

Checks whether the collator looks at no element of the pattern, `size() == 0`: an empty pattern, or one of what the
collator does not compare — an accent on its own at primary strength. The two are searched differently: an empty
pattern is found where it is looked for, the other is found nowhere, since it would otherwise be found everywhere.

## Parameters

None.

## Return value

`true` when the collator looks at nothing of the pattern, `false` otherwise.

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
    txt::collator primary{txt::strength::primary};
    txt::collated_text text(primary, "résumé");
    txt::collated_searcher none(primary, "");
    txt::collated_searcher accent(primary, "\u0301");
    println("{} {}", none.empty(), accent.empty());
    println("{} {}", text.contains(none), text.contains(accent));
}
```

Output:

```text
true true
true false
```

## See also

- [size](size.md): the number of elements
- [sgcl::txt::collated_searcher](../collated_searcher.md)
