[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md) › [jwk](jose-jwk/README.md)

# sgcl::crypto::jose::jwk::options

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jwk {
    public:
        struct options {
            string kid;
            optional<algorithm> alg;
            string use;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jwk::options` are the members a key made by the program carries
([constructor](jose-jwk/jose-jwk.md), [symmetric](jose-jwk/symmetric.md), [generate](jose-jwk/generate.md)): written
in its
JSON, and they rule what the key may do.

## Member objects

| Member | Description |
|---|---|
| `kid` | `kid`, the key's name, written in the header of what it signs or encrypts; empty, the default: none |
| `alg` | `alg`, the one algorithm the key works with; `nullopt`, the default: any of its kind ([generate](jose-jwk/generate.md) sets the one it was made for) |
| `use` | `use`: `sig` for a key that signs, `enc` for one that encrypts; empty, the default: both |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::jose::jwk key(crypto::ed25519::private_key::generate(),
                          {.kid = "signing-2026", .alg = crypto::jose::algorithm::eddsa, .use = "sig"});
    println("{} {} {}", key.kid(), key.parameters()["alg"].as_string("?"), key.use());
}
```

Output:

```text
signing-2026 EdDSA sig
```

## See also

- [jwk](jose-jwk/README.md)
- [sgcl::crypto::jose](jose.md)
