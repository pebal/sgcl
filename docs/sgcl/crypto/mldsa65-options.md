[sgcl](../README.md) › [crypto](README.md) › [mldsa65](mldsa.md)

# sgcl::crypto::mldsa65::options

```cpp
#include "sgcl/crypto/mldsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mldsa65 {
    struct options {
        slice<const byte> context;
        bool deterministic = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::mldsa65::options` is what a signature is made or checked with besides the message, Go's
`mldsa.Options` and the choice between `Sign` and `SignDeterministic`. One type serves the three parameter sets:
`mldsa44::options` and `mldsa87::options` name it too. Every field has a default:
`key.sign(m, {.context = "app/v1"})`.

## Member objects

| Member | Description |
|---|---|
| `context` | the context string of FIPS 204 §5.2, at most 255 bytes, which binds a signature to its use: one made under a context verifies only under the same; empty, the default |
| `deterministic` | [sign](mldsa65-private_key/sign.md)'s: the signing randomness all zeros, so that one message gives one signature (FIPS 204 §3.4); `false`, the default, is hedged, 32 bytes of [crypto::random](random/README.md) a signature. [verify](mldsa65-public_key/verify.md) does not read it |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
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

- [private_key::sign](mldsa65-private_key/sign.md), [public_key::verify](mldsa65-public_key/verify.md)
- [ML-DSA](mldsa.md)
