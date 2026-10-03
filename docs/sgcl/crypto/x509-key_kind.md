[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::key_kind

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class key_kind : uint8_t {
        none = 0,
        rsa,
        p256,
        p384,
        ed25519
    };
}
```

What a certificate's [public key](x509-public_key/README.md) is: one of the module's key types, or none. The value is the
index of the key in the public key's `value()`.

| Value | Description |
|---|---|
| `none` | an algorithm the module has no type for (DSA, X25519, P-521, ML-DSA), or a key its type refuses |
| `rsa` | an [rsa::public_key](rsa-public_key/README.md) |
| `p256` | a [p256::public_key](p256-public_key/README.md) |
| `p384` | a p384::public_key ([p384](p384.md)) |
| `ed25519` | an [ed25519::public_key](ed25519-public_key/README.md) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{}", root.public_key().kind() == crypto::x509::key_kind::p256);
}
```

Output:

```text
true
```

## See also

- [public_key::kind](x509-public_key/kind.md)
- [sgcl::crypto::x509](x509.md)
