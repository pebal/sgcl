[sgcl](../README.md) › [crypto](README.md) › [smime](smime.md)

# sgcl::crypto::smime::verify

```cpp
expected<cms::verified, error> verify(const slice<const byte>& message, const cms::verify_options& o) noexcept;
```

A signed entity verified: a `multipart/signed` — its first part's bytes as they stand, line ends made CRLF,
verified against the detached SignedData of its second ([cms::verify_detached](cms-verify_detached.md)) — or an
`application/pkcs7-mime` of signed-data ([cms::verify](cms-verify.md)). The message may be a whole mail whose top
entity is signed, or a part of one.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the signed entity's bytes |
| `o` | the signer's chain's options ([cms::verify_options](cms-verify_options.md)) |

## Return value

The [cms::verified](cms-verified.md): its content the signed entity; or an error: `errc::malformed` for an entity that is neither, the errors of [cms::verify](cms-verify.md) otherwise.

## Complexity

Linear in the length of the message, and one signature and chain verified.

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
    auto signed_ = crypto::smime::sign(entity, alice, key);
    auto tampered = signed_;
    auto at = std::string_view(reinterpret_cast<const char*>(tampered.data()), tampered.size()).find("ready");
    tampered[at] = byte('R');
    println("{} {}", bool(crypto::smime::verify(signed_, trust)), bool(crypto::smime::verify(tampered, trust)));
}
```

Output:

```text
true false
```

## See also

- [sign](smime-sign.md)
- [sgcl::crypto::smime](smime.md)
