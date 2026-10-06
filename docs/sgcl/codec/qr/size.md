[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::size

```cpp
uint32_t size() const noexcept;
```

The modules on a side of the symbol, 17 + 4 · version: 21 for version 1, 177 for version 40. The quiet zone around it is not counted.

## Parameters

None.

## Return value

17 + 4 · [version](version.md).

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
    println("{}", codec::qr::encode("SGCL")->size());
    println("{}", codec::qr::encode("SGCL", {.min_version = 40})->size());
}
```

Output:

```text
21
177
```

## See also

- [dark](dark.md): the modules
- [sgcl::codec::qr](README.md)
