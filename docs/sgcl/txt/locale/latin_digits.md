[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::latin_digits

```cpp
constexpr bool latin_digits() const noexcept;
```

Checks whether the tag asked for the Latin digits, `-u-nu-latn`: a [number_format](../number_format/README.md) then
writes `0123456789` where the locale's own digits are others (Arabic in Egypt, Bengali, Marathi).

## Parameters

None.

## Return value

`true` when the tag had `-u-nu-latn`.

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
    println("{} {}", txt::format_number(1234, txt::locale("ar-EG")),
            txt::format_number(1234, txt::locale("ar-EG-u-nu-latn")));
    println("{}", txt::locale("ar-EG-u-nu-latn").latin_digits());
}
```

Output:

```text
١٬٢٣٤ 1,234
true
```

## See also

- [format_number](../format_number.md)
- [sgcl::txt::locale](README.md)
