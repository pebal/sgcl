[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::root

```cpp
static constexpr locale root() noexcept;
```

Returns the root locale, no language: the case mappings of Unicode with no language's rule, the order of the collation element table without a tailoring. A default-constructed locale and an unknown tag are it.

## Parameters

None.

## Return value

The locale.

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
    constexpr auto r = txt::locale::root();
    println("{} {} {}", r == txt::locale(), r.subtag(), txt::to_upper_full("i", r));
}
```

Output:

```text
true 0 I
```

## See also

- [(constructor)](locale.md)
- [turkish](turkish.md)
- [azerbaijani](azerbaijani.md)
- [lithuanian](lithuanian.md)
- [sgcl::txt::locale](README.md)
