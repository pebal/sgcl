[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::compare

```cpp
int compare(const string& a, const string& b) const noexcept;
```

Compares two texts in the order of the collator: the letters first, then the accents, then the case, down to the
collator's [strength](../strength.md), with the settings it settled on. Two texts that differ only in how they were
written — `café` with one code point or with two — compare equal at every level.

The first level is walked through both texts side by side and the walk stops at the first difference, which in a
sorted list is nearly always the first letter: neither text is taken apart any further than that. What has been
read is kept, because the levels below need it when the first says nothing.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts, UTF-8 |

## Return value

Negative when `a` comes first, zero when the texts are one and the same to this collator, positive when `b` comes
first.

## Complexity

Linear in the lengths of the texts at worst; a difference in the first letters is found at once.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator c;
    println("{} {} {}", c.compare("ada", "Ala"), c.compare("Ala", "zebra"),
            c.compare("zebra", "Zebra"));
    println("{} {}", c.compare("resume", "RESUME"), c.compare("RESUME", "résumé"));
    println("{}", c.compare("café", "cafe\u0301"));
}
```

Output:

```text
-1 -1 -1
-1 -1
0
```

## See also

- [equal](equal.md): whether two texts are one and the same
- [operator()](operator_call.md): the collator as a comparator
- [key](key.md): the order as bytes
- [sgcl::txt::collator](../collator.md)
