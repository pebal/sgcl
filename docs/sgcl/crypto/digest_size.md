[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::digest_size

```cpp
#include "sgcl/crypto/hash_id.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    size_t digest_size(hash_id id);
}
```

The length in bytes of the digest `id` names: the `digest_size` of its type, 32 for `hash_id::sha256`. No hasher is
made. A protocol that reads a digest from data checks its length with it before it uses one.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the digest, one of the values of [hash_id](hash_id.md) |

## Return value

The length of the digest in bytes: 20, 28, 32, 48 or 64.

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
    for (auto id : {crypto::hash_id::sha1, crypto::hash_id::sha384, crypto::hash_id::sha3_224}) {
        println("{}", digest_size(id));
    }
    try {
        digest_size(crypto::hash_id(0));
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
20
48
28
sgcl::crypto: unknown hash_id
```

## See also

- [block_size](block_size.md): the block of the digest
- [digest](digest.md): the digest itself
- [sgcl::crypto::hash_id](hash_id.md)
