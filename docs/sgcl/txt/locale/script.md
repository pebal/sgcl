[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::script

```cpp
string script() const;
```

Returns the script subtag in title case: `"Latn"`; an empty text where the tag gave none.

## Parameters

None.

## Return value

The script, as a string.

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
    println("{}|{}", txt::locale("sr-latn-rs").script(), txt::locale("pl").script());
}
```

Output:

```text
Latn|
```

## See also

- [language](language.md), [script](script.md), [region](region.md)
- [to_string](to_string.md)
- [sgcl::txt::locale](README.md)
