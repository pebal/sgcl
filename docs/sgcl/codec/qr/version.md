[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::version

```cpp
int version() const noexcept;
```

The version of the symbol, 1 to 40: the smallest of the options' range that holds the data at its level.

## Parameters

None.

## Return value

1 to 40.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", codec::qr::encode("SGCL")->version());
    println("{}", codec::qr::encode(string(500, 'x'))->version());
}
```

Output:

```text
1
17
```

## See also

- [dark](dark.md): the modules
- [sgcl::codec::qr](README.md)
