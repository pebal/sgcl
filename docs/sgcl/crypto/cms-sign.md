[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::sign

```cpp
vector<byte> sign(const slice<const byte>& content, const x509::certificate& signer, const x509::signing_key& key);
vector<byte> sign(const slice<const byte>& content, const x509::certificate& signer, const x509::signing_key& key,
                  const sign_options& o);
```

A SignedData (RFC 5652 §5) of the content by the key of the signer's certificate, its ContentInfo in DER: the
content's digest, the content type and the signing time in signed attributes, signed by the key — RSA PKCS #1 v1.5
with SHA-256, ECDSA on P-256, P-384, P-521 with SHA-256, -384, -512, Ed25519 with SHA-512 (RFC 8419) — and the signer
named by its issuer and serial number; the content inside the SignedData or left out of it, the certificates with it.
The key is the module's of any kind it signs with, through [x509::signing_key](x509-signing_key/README.md): a typed
key, or a PKCS #12 file's ([pkcs12::signing_key](pkcs12/signing_key.md)). What `openssl cms -sign` makes.

## Parameters

| Parameter | Description |
|---|---|
| `content` | the content: bytes or text |
| `signer` | the key's certificate |
| `key` | the private key |
| `o` | detached, the certificates, the signing time ([sign_options](cms-sign_options.md)) |

## Return value

The ContentInfo of the SignedData, DER.

## Complexity

Linear in the length of the content, and one signature.

## Exceptions

`std::invalid_argument` for a certificate of another key.

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
    auto signed_ = crypto::cms::sign("the release notes", alice, key);
    auto v = crypto::cms::verify(signed_, trust);
    println("{} by {}", string(v->content), v->signer[0].subject().to_string());
}
```

Output:

```text
the release notes by CN=alice
```

## See also

- [verify](cms-verify.md), [verify_detached](cms-verify_detached.md)
- [smime::sign](smime-sign.md)
- [sgcl::crypto::cms](cms.md)
