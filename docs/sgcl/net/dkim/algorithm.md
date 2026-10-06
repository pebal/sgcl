[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md)

# sgcl::net::dkim::algorithm

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    enum class algorithm : uint8_t { rsa_sha256, ed25519_sha256 };
}
```

How a [signer](signer/README.md) signs: the algorithm of its key, `a=` of its signatures. RSA is what every receiver
checks; Ed25519 (RFC 8463) is short and fast, and checked by fewer, so a domain that wants it signs with both — two
signers, a message signed twice.

| Value | Description |
|---|---|
| `rsa_sha256` | `rsa-sha256`: RSA PKCS #1 v1.5 over SHA-256 (RFC 6376 §3.3.1), a key of 2048 bits made by [generate](signer/generate.md) |
| `ed25519_sha256` | `ed25519-sha256`: Ed25519 over the SHA-256 of the head (RFC 8463) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto s = net::dkim::signer::generate("example.com", "s2026", net::dkim::algorithm::ed25519_sha256);
    println("{}", s.algorithm() == net::dkim::algorithm::ed25519_sha256);
    println("{}", s.record().view().substr(0, 22));
}
```

Output:

```text
true
v=DKIM1; k=ed25519; p=
```

## See also

- [signer](signer/README.md)
- [dkim](README.md)
