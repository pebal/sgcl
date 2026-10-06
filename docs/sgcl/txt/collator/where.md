[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::where

```cpp
locale where() const noexcept;
```

Returns the language the collator was made with, as a locale of the language alone: the language whose order it
asked for, whether the library has one of its own for it or not ([tailored](tailored.md) says which). A collator made
with `pl-PL` has `pl`, the order being the language's; a collator of the root order has the empty locale, `locale()`.

## Parameters

None.

## Return value

The [locale](../locale/README.md).

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
    txt::collator polish(txt::locale("pl_PL.UTF-8"));
    println("{} {}", polish.where() == txt::locale("pl"), txt::collator().where() == txt::locale());
}
```

Output:

```text
true true
```

## See also

- [tailored](tailored.md): whether the language has an order of its own
- [sgcl::txt::collator](README.md)
