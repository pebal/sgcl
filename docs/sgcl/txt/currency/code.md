[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::code

```cpp
string code() const;
```

Returns the code in capitals: `"PLN"`.

## Parameters

None.

## Return value

The code; an empty text for no currency.

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
    println("[{}] [{}]", txt::currency("chf").code(), txt::currency().code());
}
```

Output:

```text
[CHF] []
```

## See also

- [parse](parse.md)
- [sgcl::txt::currency](README.md)
