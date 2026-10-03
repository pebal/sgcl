[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::azerbaijani

```cpp
static constexpr locale azerbaijani() noexcept;
```

Returns the Azerbaijani locale, `locale("az")`, which writes an `i` the Turkish way, as Unicode's SpecialCasing.txt says.

## Parameters

None.

## Return value

The locale of Azerbaijani.

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
    auto az = txt::locale::azerbaijani();
    println("{} {}", txt::to_upper_full("bakı şəhəri", az), az.dotted_i());
}
```

Output:

```text
BAKI ŞƏHƏRİ true
```

## See also

- [(constructor)](locale.md)
- [root](root.md)
- [turkish](turkish.md)
- [lithuanian](lithuanian.md)
- [sgcl::txt::locale](README.md)
