[sgcl](../README.md) › [crypto](README.md) › [cms](cms.md)

# sgcl::crypto::cms::verify_detached

```cpp
expected<verified, error> verify_detached(const slice<const byte>& signed_data, const slice<const byte>& content) noexcept;
expected<verified, error> verify_detached(const slice<const byte>& signed_data, const slice<const byte>& content,
                                          const verify_options& o) noexcept;
```

A detached SignedData (one without its content, `sign_options::detached`, S/MIME's `multipart/signed`) verified over the content given, as [verify](cms-verify.md) verifies one that carries it.

## Parameters

| Parameter | Description |
|---|---|
| `signed_data` | the ContentInfo of a detached SignedData |
| `content` | the content it signs |
| `o` | as [verify](cms-verify.md)'s ([verify_options](cms-verify_options.md)) |

## Return value

As [verify](cms-verify.md)'s; `errc::malformed` for a SignedData that carries its own content.

## Complexity

As [verify](cms-verify.md)'s.

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
    auto signature = crypto::cms::sign("an archive's bytes", alice, key, {.detached = true});
    println("{}", bool(crypto::cms::verify_detached(signature, "an archive's bytes", trust)));
    println("{}", bool(crypto::cms::verify_detached(signature, "other bytes", trust)));
}
```

Output:

```text
true
false
```

## See also

- [verify](cms-verify.md)
- [sgcl::crypto::cms](cms.md)
