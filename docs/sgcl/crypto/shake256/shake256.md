[sgcl](../../README.md) › [crypto](../README.md) › [shake256](README.md)

# sgcl::crypto::shake256::shake256

```cpp
shake256() noexcept;
shake128() noexcept;
```

Makes a sponge that has absorbed nothing, its input open: the 200 bytes of the state zero and the position at the
start of the first block. A read from it at once gives the output over the empty message. Copying one is the other way
to make one: the copy goes on from where the original stood, absorbing or reading.

## Parameters

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
    crypto::shake128 x;
    println(encoding::hex::encode(x.read(32)));
    crypto::shake256 y;
    println(encoding::hex::encode(y.read(32)));
}
```

Output:

```text
7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef26
46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f
```

## See also

- [update](update.md): absorbs bytes in
- [reset](reset.md): back to what the constructor made
- [sgcl::crypto::shake256](README.md)
