[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::by

```cpp
const collator& by() const noexcept;
```

Returns the collator the text was weighed by, the object's own copy of it: the one whose equality every search of
this text counts by, and whose searchers it takes without weighing them again.

## Parameters

None.

## Return value

The collator.

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
    txt::collated_text text(txt::collator(txt::locale("sv"), txt::strength::secondary), "Ångström");
    println("{} {}", text.by().tailored(), text.by().level() == txt::strength::secondary);
}
```

Output:

```text
true true
```

## See also

- [collated_searcher::by](../collated_searcher/by.md)
- [sgcl::txt::collated_text](../collated_text.md)
