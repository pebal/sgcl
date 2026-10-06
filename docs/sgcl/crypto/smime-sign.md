[sgcl](../README.md) › [crypto](README.md) › [smime](smime.md)

# sgcl::crypto::smime::sign

```cpp
vector<byte> sign(const slice<const byte>& entity, const x509::certificate& signer, const x509::signing_key& key);
vector<byte> sign(const slice<const byte>& entity, const x509::certificate& signer, const x509::signing_key& key,
                  const cms::sign_options& o);
```

A MIME entity signed: the bytes of a `multipart/signed` (RFC 1847, RFC 8551 §3.5.3) whose first part is the entity,
its line ends made CRLF, and whose second is its detached SignedData ([cms::sign](cms-sign.md)) as
`application/pkcs7-signature`, with `MIME-Version` and the `micalg` of the key's hash: what goes below a message's
own header fields (From, To, Subject), or is a part of a message. Mail readers verify it, and those without S/MIME
still show the first part.

## Parameters

| Parameter | Description |
|---|---|
| `entity` | the entity: its header fields, an empty line, its body |
| `signer` | the key's certificate |
| `key` | the private key |
| `o` | the certificates and the signing time ([cms::sign_options](cms-sign_options.md)); `detached` is set |

## Return value

The bytes of the `multipart/signed` entity.

## Complexity

Linear in the length of the entity, and one signature.

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
    // a MIME entity: header fields, an empty line, the body
    const string entity = "Content-Type: text/plain; charset=utf-8\r\n\r\nThe plan is ready.\r\n";
    auto signed_ = crypto::smime::sign(entity, alice, key);
    auto v = crypto::smime::verify(signed_, trust);
    println("{}", string(v->content) == entity);
}
```

Output:

```text
true
```

## See also

- [verify](smime-verify.md)
- [sgcl::crypto::smime](smime.md)
