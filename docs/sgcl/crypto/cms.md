[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::cms

```cpp
#include "sgcl/crypto/cms.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::cms {
    struct sign_options;
    struct verify_options;
    struct verified;
    enum class content_cipher : uint8_t;
    struct encrypt_options;

    vector<byte> sign(const slice<const byte>& content, const x509::certificate& signer,
                      const x509::signing_key& key, const sign_options& o = {});
    expected<verified, error> verify(const slice<const byte>& signed_data, const verify_options& o = {}) noexcept;
    expected<verified, error> verify_detached(const slice<const byte>& signed_data, const slice<const byte>& content,
                                              const verify_options& o = {}) noexcept;
    vector<byte> encrypt(const slice<const byte>& content, const x509::chain& recipients,
                         const encrypt_options& o = {});
    expected<vector<byte>, error> decrypt(const slice<const byte>& enveloped, const x509::certificate& recipient,
                                          const x509::signing_key& key) noexcept;
}
```

`sgcl::crypto::cms` is CMS, the Cryptographic Message Syntax (RFC 5652): data [signed](cms-sign.md) by the holder of a
certificate and [verified](cms-verify.md) by anyone who trusts its issuer, and data [encrypted](cms-encrypt.md) to the
holders of certificates and [decrypted](cms-decrypt.md) by them — what S/MIME ([smime](smime.md)), signed firmware and
packages, PDF and code signatures, and time stamps are made of. The keys are the module's, of every kind, through
[x509::signing_key](x509-signing_key/README.md), a typed key or a [PKCS #12](pkcs12/README.md) file's. Go's standard
library has no CMS; this is what `go.mozilla.org/pkcs7` and `smallstep/pkcs7` give a Go program, and `openssl cms`
the shell. Written from the RFCs; tested both ways against OpenSSL 3.6 for every kind of key. **The implementation has
not been through an independent cryptographic audit.**

## Rules

- **Signed**: by RSA (PKCS #1 v1.5 with SHA-256), ECDSA on P-256, P-384, P-521 (SHA-256, -384, -512), Ed25519 (SHA-512,
  RFC 8419), with signed attributes (content type, message digest, signing time), the signer named by its issuer and
  serial number. Read besides: RSASSA-PSS, rsaEncryption as the signature algorithm (OpenSSL's), signatures without
  signed attributes, signers named by their subject key identifier, BER. Every signer of a SignedData must verify, and
  its chain (emailProtection asked by default).
- **Encrypted**: AES-256-GCM (AuthEnvelopedData, RFC 5083) by default, or AES-256-CBC (EnvelopedData); the content key
  to RSA by RSAES-OAEP with SHA-256, to P-256, P-384, P-521 by ephemeral-static ECDH (RFC 5753), the X9.63 KDF and AES
  key wrap. Read besides: AES-128 and -192, OAEP and the KDF of SHA-1 to SHA-512.
- **Not here**, refused by name: RSA PKCS #1 v1.5 key transport (its padding oracle; OpenSSL makes it by default for
  RSA, `-keyopt rsa_padding_mode:oaep` asks for OAEP), 3DES, RC2, SHA-1 and MD5 signatures; and not read: password and
  KEK recipients, compressed and digested data, countersignatures, attribute certificates and CRLs inside a SignedData.
- **Secrets**: the content key is made and unwrapped in plain memory; an RSA key transport that does not decrypt goes
  on with a random key (RFC 3218 §2.3.2), so that it fails as a wrong tag fails; the GCM tag and the CBC padding are
  checked in constant time.
- **Errors**: `errc::verification` for a signature or a chain that does not verify, `errc::authentication` for a
  content that does not decrypt, `errc::invalid_key` for a recipient that is not among the content's,
  `errc::unsupported` for what the module does not have, `errc::malformed` for bytes that do not read.

## Member types

| Type | Definition |
|---|---|
| [sign_options](cms-sign_options.md) | detached, the certificates carried, the signing time |
| [verify_options](cms-verify_options.md) | the signer's chain's options, signers' certificates not carried |
| [verified](cms-verified.md) | the content, the signer's chain, its signing time |
| [content_cipher](cms-content_cipher.md) | AES-256-GCM or AES-256-CBC (an enumeration) |
| [encrypt_options](cms-encrypt_options.md) | the content cipher |

## Non-member functions

| Function | Description |
|---|---|
| [sign](cms-sign.md) | a SignedData of the content |
| [verify](cms-verify.md) | the content of a SignedData, every signature and the chain verified |
| [verify_detached](cms-verify_detached.md) | a detached SignedData verified over the content given |
| [encrypt](cms-encrypt.md) | the content encrypted to the holders of certificates |
| [decrypt](cms-decrypt.md) | the content of an EnvelopedData or AuthEnvelopedData |

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

    // signed, then sealed for alice; opened, then verified
    auto sealed = crypto::cms::encrypt(crypto::cms::sign("meet at noon", alice, key), to);
    auto opened = crypto::cms::decrypt(sealed, alice, key).value();
    auto v = crypto::cms::verify(opened, trust).value();
    println("{} ({})", string(v.content), v.signer[0].subject().to_string());
}
```

Output:

```text
meet at noon (CN=alice)
```

## See also

- [smime](smime.md): the same in MIME
- [pkcs12](pkcs12/README.md): keys and certificates in a file
- [x509](x509.md), [x509::signing_key](x509-signing_key/README.md)
- [sgcl::crypto](README.md)
