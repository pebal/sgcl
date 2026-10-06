[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::value

```cpp
array<byte, 32> value() const noexcept;
```

The first 32 bytes of the output over everything hashed in so far: the hash, the MAC or the derived key, by the
mode. The root is compressed on a copy of the state, so the hasher goes on.

## Parameters

None.

## Return value

32 bytes of output.

## Complexity

Linear in the depth of the tree: one compression for each subtree on the stack, and one for the root.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::blake3 h;
    h.update("abc");
    println(encoding::hex::encode(h.value()));
    println(encoding::hex::encode(crypto::blake3().value()));
}
```

Output:

```text
6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262
```

## See also

- [value_to](value_to.md): any length of output, from any position
- [verify](verify.md): a received tag checked in constant time
- [sgcl::crypto::blake3](README.md)
