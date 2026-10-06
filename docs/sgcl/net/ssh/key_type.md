[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::key_type

```cpp
#include "sgcl/net/ssh/keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    enum class key_type : uint8_t {
        ed25519,
        ecdsa_p256,
        ecdsa_p384,
        rsa,
        ecdsa_p521,
    };
}
```

The kinds of key SSH signs with here, for host keys and users' keys alike, and the kinds a certificate certifies.
DSA and the security keys (sk-*) are not read.

| Value | Description |
|---|---|
| `ed25519` | ssh-ed25519 (RFC 8709): Ed25519, what ssh-keygen makes by default |
| `ecdsa_p256` | ecdsa-sha2-nistp256 (RFC 5656): ECDSA on P-256 with SHA-256 |
| `ecdsa_p384` | ecdsa-sha2-nistp384: ECDSA on P-384 with SHA-384 |
| `rsa` | ssh-rsa keys, signing rsa-sha2-512 and rsa-sha2-256 (RFC 8332), never SHA-1 |
| `ecdsa_p521` | ecdsa-sha2-nistp521: ECDSA on P-521 with SHA-512 |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::generate(net::ssh::key_type::ecdsa_p256);
    println("{} {}", key.type() == net::ssh::key_type::ecdsa_p256, key.public_key().type_name());
}
```

Output:

```text
true ecdsa-sha2-nistp256
```

## See also

- [private_key::generate](private_key/generate.md), [public_key::type](public_key/type.md)
- [net::ssh](README.md)
