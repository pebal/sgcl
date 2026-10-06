[sgcl](../README.md) › [crypto](README.md) › [smime](smime.md)

# sgcl::crypto::smime::decrypt

```cpp
expected<vector<byte>, error> decrypt(const slice<const byte>& message, const x509::certificate& recipient,
                                      const x509::signing_key& key) noexcept;
```

The entity inside an `application/pkcs7-mime` of enveloped-data or authEnveloped-data, for the recipient whose certificate and key are given ([cms::decrypt](cms-decrypt.md)): what OpenSSL, Thunderbird and Outlook seal.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the sealed entity's bytes, or a whole mail whose top entity it is |
| `recipient` | the recipient's certificate |
| `key` | its private key |

## Return value

The entity, or an error: `errc::malformed` for another entity, the errors of [cms::decrypt](cms-decrypt.md) otherwise.

## Complexity

Linear in the length of the message.

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
    // a MIME entity: header fields, an empty line, the body
    const string entity = "Content-Type: text/plain; charset=utf-8\r\n\r\nThe plan is ready.\r\n";
    println("{}", crypto::smime::decrypt(entity, alice, key).error().message());
}
```

Output:

```text
sgcl::crypto::smime: not application/pkcs7-mime
```

## See also

- [encrypt](smime-encrypt.md)
- [sgcl::crypto::smime](smime.md)
