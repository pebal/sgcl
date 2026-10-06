[sgcl](../README.md) › [crypto](README.md) › [slhdsa_sha2_128s](slhdsa.md)

# sgcl::crypto::slhdsa_sha2_128s::options

```cpp
#include "sgcl/crypto/slhdsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::slhdsa_sha2_128s {
    struct options {
        slice<const byte> context;
        bool deterministic = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::slhdsa_sha2_128s::options` is what a signature is made or checked with besides the message. One type serves the
twelve parameter sets: `options` of every `slhdsa_` namespace names it. Every field has a default:
`key.sign(m, {.context = "app/v1"})`.

## Member objects

| Member | Description |
|---|---|
| `context` | the context string of FIPS 205 §10.2, at most 255 bytes, which binds a signature to its use: one made under a context verifies only under the same; empty, the default |
| `deterministic` | [sign](slhdsa_sha2_128s-private_key/sign.md)'s: the randomness PK.seed (§10.2.1), so that one message gives one signature; `false`, the default, is hedged, n bytes of [crypto::random](random/README.md) a signature. [verify](slhdsa_sha2_128s-public_key/verify.md) does not read it |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto sig = key.sign("a message", {.context = "invoices"});
    auto pub = key.public_key();
    println("{} {}", pub.verify("a message", sig, {.context = "invoices"}), pub.verify("a message", sig, {.context = "orders"}));
}
```

Output:

```text
true false
```

## See also

- [private_key::sign](slhdsa_sha2_128s-private_key/sign.md), [public_key::verify](slhdsa_sha2_128s-public_key/verify.md)
- [SLH-DSA](slhdsa.md)
