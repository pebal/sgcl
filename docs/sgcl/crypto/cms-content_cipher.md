[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::content_cipher

```cpp
#include "sgcl/crypto/cms.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::cms {
    enum class content_cipher : uint8_t {
        aes256_gcm,
        aes256_cbc
    };
}
```

`sgcl::crypto::cms::content_cipher` is the cipher of an encrypted content ([encrypt_options](cms-encrypt_options.md)).

| Value | Description |
|---|---|
| `aes256_gcm` | AES-256-GCM in an AuthEnvelopedData (RFC 5083): authenticated, S/MIME 4.0's (RFC 8551); the default |
| `aes256_cbc` | AES-256-CBC in an EnvelopedData: for readers without AuthEnvelopedData; not authenticated, so a changed ciphertext may decrypt to other bytes (sign it as well) |

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
    auto sealed = crypto::cms::encrypt("m", to, {.cipher = crypto::cms::content_cipher::aes256_gcm});
    println("{}", sealed.size() > 0);
}
```

Output:

```text
true
```

## See also

- [encrypt_options](cms-encrypt_options.md)
- [sgcl::crypto::cms](cms.md)
