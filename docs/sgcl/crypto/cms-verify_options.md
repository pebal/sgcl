[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::verify_options

```cpp
#include "sgcl/crypto/cms.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::cms {
    struct verify_options {
        x509::verify_options chain;
        x509::chain certificates;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::cms::verify_options` is what [verify](cms-verify.md) and [verify_detached](cms-verify_detached.md) check a signer against.

## Member objects

| Member | Description |
|---|---|
| `chain` | the signer's chain's options ([x509::verify_options](x509-verify_options.md)): the roots (the system's by default), more intermediates (the SignedData's are added), the time (now by default), the key usages (emailProtection when none are given, as OpenSSL's `cms -verify`; `ext_key_usage::any` for any) |
| `certificates` | signers' certificates the SignedData does not carry; none by default |

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
    crypto::cms::verify_options given = trust;
    given.certificates.push_back(alice);
    println("{}", bool(crypto::cms::verify(bare, given)));
}
```

Output:

```text
true
```

## See also

- [verify](cms-verify.md)
- [sgcl::crypto::cms](cms.md)
