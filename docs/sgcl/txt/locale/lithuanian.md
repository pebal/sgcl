[sgcl](../../README.md) › [txt](../README.md) › [locale](../locale.md)

# sgcl::txt::locale::lithuanian

```cpp
static constexpr locale lithuanian() noexcept;
```

Returns the Lithuanian locale, `locale("lt")`: an `i` or a `j` under an accent keeps its dot above, so an `Ì` lowers to `i`, a combining dot above and a combining grave.

## Parameters

None.

## Return value

The locale of Lithuanian.

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
    auto lt = txt::locale::lithuanian();
    println("{} code points", txt::to_lower_full("Ì", lt).rune_count());
    println("{} code point", txt::to_lower_full("Ì").rune_count());
}
```

Output:

```text
3 code points
1 code point
```

## See also

- [(constructor)](locale.md)
- [root](root.md)
- [turkish](turkish.md)
- [azerbaijani](azerbaijani.md)
- [sgcl::txt::locale](../locale.md)
