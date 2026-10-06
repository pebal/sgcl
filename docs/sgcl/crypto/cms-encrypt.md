[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::encrypt

```cpp
vector<byte> encrypt(const slice<const byte>& content, const x509::chain& recipients);
vector<byte> encrypt(const slice<const byte>& content, const x509::chain& recipients, const encrypt_options& o);
```

The content encrypted to the holders of the certificates, its ContentInfo in DER: an AuthEnvelopedData of
AES-256-GCM (RFC 5083, S/MIME 4.0's) or an EnvelopedData of AES-256-CBC ([encrypt_options](cms-encrypt_options.md)),
one random content key reaching each recipient — an RSA certificate's by RSAES-OAEP with SHA-256, a P-256, P-384 or
P-521 one's by ephemeral-static ECDH (RFC 5753) with the X9.63 KDF of the curve's hash and AES-256 key wrap. What
`openssl cms -encrypt` reads (and makes, with OAEP asked for RSA).

## Parameters

| Parameter | Description |
|---|---|
| `content` | the content: bytes or text |
| `recipients` | their certificates; each one decrypts |
| `o` | the content cipher ([encrypt_options](cms-encrypt_options.md)) |

## Return value

The ContentInfo, DER.

## Complexity

Linear in the length of the content, and a key transport or agreement a recipient.

## Exceptions

`std::invalid_argument` for no recipient, or a certificate whose key encrypts nothing (Ed25519, a kind the module does not have).

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
    auto sealed = crypto::cms::encrypt("for alice only", to);
    println("{}", string(crypto::cms::decrypt(sealed, alice, key).value()));
}
```

Output:

```text
for alice only
```

## See also

- [decrypt](cms-decrypt.md)
- [smime::encrypt](smime-encrypt.md)
- [sgcl::crypto::cms](cms.md)
