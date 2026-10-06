[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::verify

```cpp
expected<verified, error> verify(const slice<const byte>& signed_data) noexcept;
expected<verified, error> verify(const slice<const byte>& signed_data, const verify_options& o) noexcept;
```

The content of a SignedData (DER, or the BER OpenSSL streams) whose every signer's signature verifies: each
signer's certificate found among those the SignedData carries or `o.certificates`, the content type and the message
digest of its signed attributes checked against the content, its signature checked under the certificate's key
(RSA PKCS #1 v1.5 and PSS, ECDSA on the three curves, Ed25519; signatures without signed attributes too), and its
chain verified under `o.chain` — the roots, more intermediates, the time (now by default, never the signer's own
claim) and the key usages, emailProtection when none are asked, as OpenSSL's `cms -verify`.

## Parameters

| Parameter | Description |
|---|---|
| `signed_data` | the ContentInfo of a SignedData carrying its content |
| `o` | the chain's options and more certificates ([verify_options](cms-verify_options.md)); the system's roots by default |

## Return value

The [verified](cms-verified.md) content, the first signer's chain and its signing time, or an error:
`errc::verification` for a signature, a digest or a chain that does not verify (a SHA-1 signature among them), or a
signer whose certificate is nowhere; `errc::unsupported` for an algorithm the module does not have; `errc::malformed`
for bytes that do not read, and for a detached SignedData ([verify_detached](cms-verify_detached.md) reads those).

## Complexity

Linear in the length of the content, one signature and one chain verified a signer.

## Exceptions

None.

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
    auto signed_ = crypto::cms::sign("hello", alice, key);
    println("{}", bool(crypto::cms::verify(signed_, trust)));
    signed_[60] ^= byte(1);   // a byte of the signed part changed
    println("{}", bool(crypto::cms::verify(signed_, trust)));
}
```

Output:

```text
true
false
```

## See also

- [sign](cms-sign.md), [verify_detached](cms-verify_detached.md)
- [sgcl::crypto::cms](cms.md)
