[sgcl](../../README.md) › [txt](../README.md) › [region](README.md)

# sgcl::txt::region::code

```cpp
string code() const;
```

Returns the code: `"PL"`.

## Parameters

None.

## Return value

The code; an empty text for none.

## Complexity

Constant.

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
    println("[{}] [{}]", txt::region("de").code(), txt::region().code());
}
```

Output:

```text
[DE] []
```

## See also

- [display_name](display_name.md)
- [sgcl::txt::region](README.md)
