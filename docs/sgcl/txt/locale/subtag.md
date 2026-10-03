[sgcl](../../README.md) › [txt](../README.md) › [locale](../locale.md)

# sgcl::txt::locale::subtag

```cpp
constexpr uint32_t subtag() const noexcept;
```

Returns the language subtag in four bytes, its letters lower-cased, one a byte from the most significant used: what a table keyed by language is looked up with (the collator's tailorings are the one such table in the library).

## Parameters

None.

## Return value

The packed subtag; 0 for the root locale.

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
    println("{:#x} {:#x} {}", txt::locale("pl-PL").subtag(), txt::locale("FIL").subtag(),
            txt::locale().subtag());
}
```

Output:

```text
0x706c 0x66696c 0
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::txt::locale](../locale.md)
