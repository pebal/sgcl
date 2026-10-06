[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::language

```cpp
string language() const;
```

Returns the language subtag, lower-cased: `"sr"`; an empty text for the root locale and `und`.

## Parameters

None.

## Return value

The language, as a string.

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
    println("{}|{}", txt::locale("sr-Latn-RS").language(), txt::locale("pl").language());
}
```

Output:

```text
sr|pl
```

## See also

- [language](language.md), [script](script.md), [region](region.md)
- [to_string](to_string.md)
- [sgcl::txt::locale](README.md)
