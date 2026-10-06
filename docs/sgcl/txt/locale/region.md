[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::region

```cpp
string region() const;
```

Returns the region subtag, two capitals or three digits: `"RS"`, `"419"`; an empty text where the tag gave none.

## Parameters

None.

## Return value

The region, as a string.

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
    println("{}|{}", txt::locale("es_419").region(), txt::locale("pl").region());
}
```

Output:

```text
419|
```

## See also

- [language](language.md), [script](script.md), [region](region.md)
- [to_string](to_string.md)
- [sgcl::txt::locale](README.md)
