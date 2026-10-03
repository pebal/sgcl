[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](../collated_matches.md)

# sgcl::txt::collated_matches::count

```cpp
size_type count() const noexcept;
```

Returns the number of occurrences, which do not overlap: walked and counted, not stored, over the text weighed
once. It is the count [collated_text::count](../collated_text/count.md) gives of the same text and pattern.

## Parameters

None.

## Return value

The number of occurrences; 0 for an empty pattern and a range made by the default constructor.

## Complexity

A scan of the weighed text: linear on ordinary text, the text times the pattern at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator shifted{{.strength = txt::strength::primary,
                           .punctuation = txt::punctuation::shifted}};
    println("{}", txt::collated_matches(shifted, "e-mail, email, E-Mail, mail", "email").count());
}
```

Output:

```text
3
```

## See also

- [empty](empty.md): whether there is no occurrence
- [sgcl::txt::collated_matches](../collated_matches.md)
