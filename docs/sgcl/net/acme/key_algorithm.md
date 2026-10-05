[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::key_algorithm

```cpp
#include "sgcl/net/acme/key.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    enum class key_algorithm : uint8_t {
        es256,
        es384,
        eddsa,
        rs256,
    };
}
```

The JWS algorithms an [account_key](account_key/README.md) signs with (RFC 7518 §3.1, RFC 8037 §3.1), each of one kind
of key; and the kind of a [manager](manager/README.md)'s certificate keys.

| Value | Description |
|---|---|
| `es256` | ECDSA on P-256 over SHA-256: the default, what Let's Encrypt and Go's autocert use |
| `es384` | ECDSA on P-384 over SHA-384 |
| `eddsa` | Ed25519, where the CA takes it (Let's Encrypt does not, Pebble does) |
| `rs256` | RSA PKCS #1 v1.5 over SHA-256, for an existing RSA key (a new one is 2048 bits) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key key(net::acme::key_algorithm::eddsa);
    println("{}", key.algorithm() == net::acme::key_algorithm::eddsa);
    println("{}", key.jwk().starts_with(R"({"crv":"Ed25519","kty":"OKP")"));
}
```

Output:

```text
true
true
```

## See also

- [account_key](account_key/README.md)
- [net::acme](README.md)
