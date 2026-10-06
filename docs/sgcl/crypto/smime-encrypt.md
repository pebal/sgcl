[sgcl](../README.md) › [crypto](README.md) › [smime](smime.md)

# sgcl::crypto::smime::encrypt

```cpp
vector<byte> encrypt(const slice<const byte>& entity, const x509::chain& recipients);
vector<byte> encrypt(const slice<const byte>& entity, const x509::chain& recipients, const cms::encrypt_options& o);
```

A MIME entity encrypted to the holders of the certificates: the bytes of an `application/pkcs7-mime` of
`smime-type=authEnveloped-data` (AES-256-GCM, RFC 8551 §3.3), or `enveloped-data` with AES-256-CBC, base64 in lines of
64, its content the entity with its line ends made CRLF ([cms::encrypt](cms-encrypt.md)).

## Parameters

| Parameter | Description |
|---|---|
| `entity` | the entity: its header fields, an empty line, its body |
| `recipients` | their certificates |
| `o` | the content cipher ([cms::encrypt_options](cms-encrypt_options.md)) |

## Return value

The bytes of the `application/pkcs7-mime` entity.

## Complexity

Linear in the length of the entity, and a key transport or agreement a recipient.

## Exceptions

`std::invalid_argument` for no recipient or a certificate whose key encrypts nothing.

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
    auto sealed = crypto::smime::encrypt(entity, to);
    println("{}", string(crypto::smime::decrypt(sealed, alice, key).value()) == entity);
}
```

Output:

```text
true
```

## See also

- [decrypt](smime-decrypt.md)
- [sgcl::crypto::smime](smime.md)
