[sgcl](../../README.md) › [crypto](../README.md) › [sha3_256](../sha3_256.md)

# sgcl::crypto::sha3_256::reset

```cpp
void reset() noexcept;
```

Puts the hasher back to what the constructor made: the state zeroed and the position at the start of the first
block. One hasher then serves message after message.

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
    crypto::sha3_512 h;
    h.update("abc");
    h.reset();
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26
```

## See also

- [(constructor)](sha3_256.md): a hasher of nothing yet
- [sgcl::crypto::sha3_256](../sha3_256.md)
