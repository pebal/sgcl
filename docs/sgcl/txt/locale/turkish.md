[sgcl](../../README.md) › [txt](../README.md) › [locale](../locale.md)

# sgcl::txt::locale::turkish

```cpp
static constexpr locale turkish() noexcept;
```

Returns the Turkish locale, `locale("tr")`: an `i` keeps its dot in upper case (`İ`) and an `I` has none in lower case (`ı`); an `I` before a combining dot above is a dotted `i`.

## Parameters

None.

## Return value

The locale of Turkish.

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
    auto tr = txt::locale::turkish();
    println("{} {}", txt::to_upper_full("istanbul", tr), txt::to_lower_full("DİYARBAKIR", tr));
}
```

Output:

```text
İSTANBUL diyarbakır
```

## See also

- [(constructor)](locale.md)
- [root](root.md)
- [azerbaijani](azerbaijani.md)
- [lithuanian](lithuanian.md)
- [sgcl::txt::locale](../locale.md)
