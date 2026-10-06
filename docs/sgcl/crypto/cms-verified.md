[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::verified

```cpp
#include "sgcl/crypto/cms.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::cms {
    struct verified {
        vector<byte> content;
        x509::chain signer;
        optional<time::datetime> signing_time;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::cms::verified` is what a verification gives.

## Member objects

| Member | Description |
|---|---|
| `content` | the content signed |
| `signer` | the first signer's chain as it verified, the signer's certificate first |
| `signing_time` | the first signer's signingTime, when it gave one: a claim of the signer, not a time stamp |

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
    auto when = time::datetime::from_unix(1800000000, time::zone::utc());
    auto v = crypto::cms::verify(crypto::cms::sign("m", alice, key, {.signing_time = when}), trust);
    println("{} {} {}", string(v->content), v->signer.size(), v->signing_time->unix());
}
```

Output:

```text
m 2 1800000000
```

## See also

- [verify](cms-verify.md)
- [sgcl::crypto::cms](cms.md)
