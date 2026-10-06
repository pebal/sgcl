[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::has_names

```cpp
bool has_names() const noexcept;
```

Checks whether the display names of this locale are included: its header of sgcl/txt/names/ or the header of one of
its CLDR parents (`de-AT` has them when `de.h` is included).

## Parameters

None.

## Return value

`true` when a name asked in this locale comes from data rather than the codes.

## Complexity

Linear in the number of included locales.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::locale("pl").has_names(), txt::locale("de-AT").has_names(),
            txt::locale("it").has_names());
}
```

Output:

```text
true true false
```

## See also

- [display_name](display_name.md)
- [display names](../names.md)
- [sgcl::txt::locale](README.md)
