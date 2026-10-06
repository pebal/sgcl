[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::encrypt_options

```cpp
#include "sgcl/crypto/cms.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::cms {
    struct encrypt_options {
        content_cipher cipher = content_cipher::aes256_gcm;
    };
}
```

`sgcl::crypto::cms::encrypt_options` is how [encrypt](cms-encrypt.md) encrypts. `cms::encrypt(content, to, {.cipher = cms::content_cipher::aes256_cbc})`.

## Member objects

| Member | Description |
|---|---|
| `cipher` | the content's cipher ([content_cipher](cms-content_cipher.md)): AES-256-GCM in an AuthEnvelopedData by default |

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
    crypto::x509::chain to;
    to.push_back(alice);
    auto old = crypto::cms::encrypt("m", to, {.cipher = crypto::cms::content_cipher::aes256_cbc});
    println("{}", string(crypto::cms::decrypt(old, alice, key).value()));
}
```

Output:

```text
m
```

## See also

- [encrypt](cms-encrypt.md)
- [sgcl::crypto::cms](cms.md)
