[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::text

```cpp
slice<const char> text() const noexcept;
```

Returns the text the object was built from, the positions of its matches being bytes of it: a slice made of the
string it holds when it is asked for, since the object keeps the string and not a slice ([bytes](bytes.md)).

## Parameters

None.

## Return value

The text, as a slice; an empty slice for an object made by the default constructor.

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
    txt::collated_text weighed(txt::collator(txt::strength::primary), "Tytuł: RÉSUMÉ");
    auto m = weighed.find("resume");
    println("{}", weighed.text().subslice(m->at, m->size));
}
```

Output:

```text
RÉSUMÉ
```

## See also

- [bytes](bytes.md): the same text as a string
- [sgcl::txt::collated_text](../collated_text.md)
