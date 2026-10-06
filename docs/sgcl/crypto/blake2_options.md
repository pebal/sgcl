[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::blake2_options

```cpp
#include "sgcl/crypto/blake2.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    struct blake2_options {
        slice<const byte> key;
        slice<const byte> salt;
        slice<const byte> personalization;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

What a [BLAKE2 hasher](blake2b_512/README.md) is made with beyond its length: the parameters of RFC 7693's parameter
block that a program sets. A key makes the digest a MAC; a salt and a personalization make the digests of one input
differ by use, so that a digest computed for one purpose is never taken for another's. All three empty is the plain
hash. The options are read by the constructor and not kept: the hasher keeps what the key makes of its state, and the
slices may die after the call.

## Member objects

| Member | Description |
|---|---|
| `key` | the secret key, up to `max_key_size` bytes: 64 for BLAKE2b, 32 for BLAKE2s; empty, the default, for no key |
| `salt` | a salt of up to 16 bytes (BLAKE2s: 8), zero-padded to its field; empty by default |
| `personalization` | a personalization of up to 16 bytes (BLAKE2s: 8), zero-padded to its field, as Python's `hashlib` pads it; empty by default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // one input, two uses: two digests that never meet
    println(encoding::hex::encode(crypto::blake2b_256::of("abc", {.personalization = "MyApp v1 signing"})));
    println(encoding::hex::encode(crypto::blake2b_256::of("abc", {.personalization = "MyApp v1 storage"})));
}
```

Output:

```text
e0a60f51a461a741eb461090ec687576dc6d29d4f6b1aec3765604bb49c99b56
e7905ec4bc82a9f02511d27403c1764d16762e590e65894ec3ff897df9e896b6
```

## See also

- [blake2b_512](blake2b_512/README.md): the hashers that take them
- [The module](README.md)
