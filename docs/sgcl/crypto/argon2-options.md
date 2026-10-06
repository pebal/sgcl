[sgcl](../README.md) › [crypto](README.md) › [argon2](argon2/README.md)

# sgcl::crypto::argon2::options

```cpp
#include "sgcl/crypto/argon2.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class argon2 {
    public:
        struct options {
            argon2::variant variant = argon2::variant::id;
            uint32_t memory = 65536;
            uint32_t iterations = 3;
            uint32_t parallelism = 4;
            slice<const byte> secret;
            slice<const byte> associated_data;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

The variant and the costs of an [argon2](argon2/README.md) hash, and the two optional inputs of RFC 9106. The defaults
are the RFC's second recommended setting (§4), for a machine without the 2 GiB of its first: Argon2id with 64 MiB of
memory, three passes and four lanes, about 50 ms on a core of an Apple M2 with its lanes on the scheduler's workers.

## Member objects

| Member | Description |
|---|---|
| `variant` | the [variant](argon2-variant.md), `variant::id` by default |
| `memory` | the memory in KiB, `m`: at least 8 per lane, rounded down to a multiple of 4 per lane; 65536 (64 MiB) by default |
| `iterations` | the passes over the memory, `t`: at least 1; 3 by default |
| `parallelism` | the lanes, `p`: 1 to 2^24 - 1, run side by side on the scheduler's workers; 4 by default. The lanes are part of the function: a hash made with 4 is not the one made with 1 |
| `secret` | `K`, a secret key hashed in (a "pepper" kept apart from the stored hashes, in the program's configuration); empty by default. [verify](argon2/verify.md) takes it again |
| `associated_data` | `X`, data bound into the hash; empty by default. A PHC string has no place for it: [generate](argon2/generate.md) refuses it |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // OWASP's lighter setting: 19 MiB, two passes, one lane
    crypto::argon2::options light{.memory = 19456, .iterations = 2, .parallelism = 1};
    println(encoding::hex::encode(crypto::argon2::derive("password", "somesalt", 16, light)));
}
```

Output:

```text
245637c6eb90dda488179f2b39ea72c8
```

## See also

- [argon2](argon2/README.md): what takes them
- [argon2::variant](argon2-variant.md): the three variants
- [The module](README.md)
