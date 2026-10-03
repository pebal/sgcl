[sgcl](../../README.md) › [crypto](../README.md) › [shake256](../shake256.md)

# sgcl::crypto::shake256::reset

```cpp
void reset() noexcept;
```

Puts the sponge back to what the constructor made: the state zeroed, the position at the start, and the input open
again, whether it was read or not. One object then serves seed after seed.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::shake256 x;
    x.update("seed");
    println(encoding::hex::encode(x.read(32)));
    x.reset();
    x.update("abc");  // open again after the read
    println(encoding::hex::encode(x.read(32)));
}
```

Output:

```text
4fd6800b5ddf65323de29f59e5da90d3fa6778594e60e2ff4326622eff3e42c4
483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739
```

## See also

- [update](update.md): what a read closes
- [(constructor)](shake256.md): a sponge with its input open
- [sgcl::crypto::shake256](../shake256.md)
