[sgcl](../README.md) › [crypto](README.md) › [jose](jose.md)

# sgcl::crypto::jose::key_type

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    enum class key_type : uint8_t {
        ec = 1,
        rsa,
        oct,
        okp
    };
}
```

The `kty` of a [jwk](jose-jwk/README.md) (RFC 7518 §6.1, RFC 8037 §2): the kind of its key.

| Value | Description |
|---|---|
| `ec` | EC: a P-256, P-384 or P-521 key |
| `rsa` | RSA |
| `oct` | oct: a symmetric key, its octets (an HMAC key, a key wrap's, a `dir` content key) |
| `okp` | OKP: an Ed25519 or X25519 key |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::jose::jwk key(crypto::x25519::private_key::generate());
    println("{} {}", key.type() == crypto::jose::key_type::okp, key.crv());
}
```

Output:

```text
true X25519
```

## See also

- [jwk::type](jose-jwk/type.md)
- [sgcl::crypto::jose](jose.md)
