[sgcl](../../README.md) › [txt](../README.md) › [collated_text](../collated_text.md)

# sgcl::txt::collated_text::size

```cpp
size_t size() const noexcept;
```

Returns the number of elements the text weighed to — every element of the algorithm, those the collator looks at
and those it does not — which is neither its bytes nor its letters: a letter with an accent is two elements, a
contraction one.

## Parameters

None.

## Return value

The number of elements.

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
    txt::collator root;
    txt::collator czech(txt::locale("cs"));
    println("{} {} {}", txt::collated_text(root, "abc").size(),
            txt::collated_text(root, "é").size(), txt::collated_text(czech, "ch").size());
}
```

Output:

```text
3 2 1
```

## See also

- [at](at.md): where each element came from
- [sgcl::txt::collated_text](../collated_text.md)
