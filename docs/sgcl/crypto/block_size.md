[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::block_size

```cpp
#include "sgcl/crypto/hash_id.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    size_t block_size(hash_id id);
}
```

The block in bytes of the digest `id` names: the `block_size` of its type, 64 for `hash_id::sha256`, and for SHA-3 the
rate, 136 for `hash_id::sha3_256`. It is what HMAC pads its key to, and what a protocol that builds an HMAC of a
digest named at run time needs. No hasher is made.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the digest, one of the values of [hash_id](hash_id.md) |

## Return value

The block in bytes: 64 for SHA-1, SHA-224 and SHA-256, 128 for SHA-384, SHA-512 and SHA-512/256, 144, 136, 104 and 72
for SHA3-224, -256, -384 and -512.

## Complexity

Constant.

## Exceptions

`std::invalid_argument` when `id` is none of the values of `hash_id` (a number cast to it).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (auto id : {crypto::hash_id::sha256, crypto::hash_id::sha512, crypto::hash_id::sha3_256}) {
        println("{}", block_size(id));
    }
}
```

Output:

```text
64
128
136
```

## See also

- [digest_size](digest_size.md): the length of the digest
- [hmac](hmac/README.md): where the block matters
- [sgcl::crypto::hash_id](hash_id.md)
