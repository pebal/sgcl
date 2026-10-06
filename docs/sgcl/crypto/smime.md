[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::smime

```cpp
#include "sgcl/crypto/smime.h"

namespace sgcl::crypto::smime {
    vector<byte> sign(const slice<const byte>& entity, const x509::certificate& signer,
                      const x509::signing_key& key, const cms::sign_options& o = {});
    expected<cms::verified, error> verify(const slice<const byte>& message, const cms::verify_options& o) noexcept;
    vector<byte> encrypt(const slice<const byte>& entity, const x509::chain& recipients,
                         const cms::encrypt_options& o = {});
    expected<vector<byte>, error> decrypt(const slice<const byte>& message, const x509::certificate& recipient,
                                          const x509::signing_key& key) noexcept;
}
```

`sgcl::crypto::smime` is S/MIME 4.0 (RFC 8551) over [CMS](cms.md): a MIME entity — its header fields, an empty line and
its body, as [encoding::email](../encoding/email/README.md) writes one and an IMAP server gives one — [signed](smime-sign.md)
into a `multipart/signed` with a detached signature or [encrypted](smime-encrypt.md) into an `application/pkcs7-mime`,
and [verified](smime-verify.md) and [decrypted](smime-decrypt.md) back. What Thunderbird, Outlook, Apple Mail and
`openssl cms`/`smime` speak.

## Rules

- **Bytes, not a parsed part**: a signature covers the entity's bytes exactly as they were written, which a parse and a
  write again would not keep; the functions take and give the entity's bytes.
- **Canonical form**: line ends are made CRLF (RFC 8551 §3.1.1) before signing, encrypting and verifying; the content of
  a verification or a decryption is the entity in that form.
- **Signed**: `multipart/signed` with `protocol="application/pkcs7-signature"` and the `micalg` of the key's hash;
  read besides: `application/pkcs7-mime` of signed-data, the `x-pkcs7` types of older writers. **Encrypted**:
  `smime-type=authEnveloped-data` (AES-256-GCM) by default, `enveloped-data` (AES-256-CBC) when asked.
- The algorithms, the keys and the errors are [cms](cms.md)'s.

## Non-member functions

| Function | Description |
|---|---|
| [sign](smime-sign.md) | an entity signed into a `multipart/signed` |
| [verify](smime-verify.md) | a signed entity verified, its content given |
| [encrypt](smime-encrypt.md) | an entity encrypted into an `application/pkcs7-mime` |
| [decrypt](smime-decrypt.md) | the entity inside an `application/pkcs7-mime` |

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
    // a MIME entity: header fields, an empty line, the body
    const string entity = "Content-Type: text/plain; charset=utf-8\r\n\r\nThe plan is ready.\r\n";
    crypto::x509::chain to;
    to.push_back(alice);

    // signed, then sealed; opened, then verified
    auto mail = crypto::smime::encrypt(crypto::smime::sign(entity, alice, key), to);
    auto opened = crypto::smime::decrypt(mail, alice, key).value();
    auto v = crypto::smime::verify(opened, trust).value();
    println("{}", string(v.content) == entity);
}
```

Output:

```text
true
```

## See also

- [cms](cms.md): the signatures and the encryption under it
- [encoding::email](../encoding/email/README.md): the messages
- [pkcs12](pkcs12/README.md): the key and the certificate in a file
- [sgcl::crypto](README.md)
