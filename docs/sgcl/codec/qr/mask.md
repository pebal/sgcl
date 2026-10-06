[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::mask

```cpp
int mask() const noexcept;
```

The mask pattern XORed into the data modules, 0 to 7: the one the options name, or the one of least penalty.

## Parameters

None.

## Return value

0 to 7.

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
    println("{}", codec::qr::encode("SGCL")->mask());
    println("{}", codec::qr::encode("SGCL", {.mask = 6})->mask());
}
```

Output:

```text
1
6
```

## See also

- [dark](dark.md): the modules
- [sgcl::codec::qr](README.md)
