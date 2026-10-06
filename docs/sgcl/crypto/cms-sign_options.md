[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::sign_options

```cpp
#include "sgcl/crypto/cms.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::cms {
    struct sign_options {
        bool detached = false;
        bool include_certificates = true;
        x509::chain certificates;
        optional<time::datetime> signing_time;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::cms::sign_options` is how a [sign](cms-sign.md) is made. Every field has a default: `cms::sign(content, cert, key, {.detached = true})`.

## Member objects

| Member | Description |
|---|---|
| `detached` | the content left out of the SignedData, sent beside it (S/MIME's `multipart/signed`, a signature of a file); `false` by default |
| `include_certificates` | the signer's certificate and `certificates` carried in the SignedData, for a verifier without them; `true` by default |
| `certificates` | more to carry: the intermediates of the signer's chain; none by default |
| `signing_time` | the signingTime attribute; `nullopt`, the default: now |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/crypto/smime.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a CA and a leaf of S/MIME it issued (a program reads its own from files or a .p12)
    auto ca_key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template ct;
    ct.common_name = "Example CA";
    ct.is_ca = true;
    auto ca = crypto::x509::create_certificate(ct, ca_key);
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template lt;
    lt.common_name = "alice";
    lt.email_addresses = {"alice@example.test"};
    lt.ext_key_usages = {crypto::x509::ext_key_usage::email_protection};
    auto alice = crypto::x509::create_certificate(lt, key.public_key().to_pkix_der(), ca, ca_key);
    crypto::cms::verify_options trust;
    trust.chain.roots = crypto::x509::certificate_pool();
    trust.chain.roots->add(ca);
    auto bare = crypto::cms::sign("m", alice, key, {.include_certificates = false});
    println("{}", crypto::cms::verify(bare, trust).error().message());
}
```

Output:

```text
sgcl::crypto::cms: the signer's certificate is neither in the SignedData nor given
```

## See also

- [sign](cms-sign.md)
- [sgcl::crypto::cms](cms.md)
