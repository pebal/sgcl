[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::key_usage

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class key_usage : uint16_t {
        digital_signature = 1 << 0,
        content_commitment = 1 << 1,
        key_encipherment = 1 << 2,
        data_encipherment = 1 << 3,
        key_agreement = 1 << 4,
        cert_sign = 1 << 5,
        crl_sign = 1 << 6,
        encipher_only = 1 << 7,
        decipher_only = 1 << 8
    };

    constexpr key_usage operator|(key_usage a, key_usage b) noexcept;
    constexpr key_usage operator&(key_usage a, key_usage b) noexcept;
}
```

The bits of a certificate's keyUsage (RFC 5280 §4.2.1.3), as flags: Go's `KeyUsage`. `|` joins them and `&` keeps
the ones two values share, so that a set of bits is written `key_usage::cert_sign | key_usage::crl_sign` and asked
for with [allows](x509-certificate/allows.md).

| Value | Description |
|---|---|
| `digital_signature` | the key verifies signatures other than of certificates and CRLs: a TLS handshake |
| `content_commitment` | the key verifies signatures that commit to content (once nonRepudiation) |
| `key_encipherment` | the key encrypts keys: RSA key transport |
| `data_encipherment` | the key encrypts data directly |
| `key_agreement` | the key agrees on keys: ECDH |
| `cert_sign` | the key verifies signatures of certificates: keyCertSign, what a CA with a keyUsage needs |
| `crl_sign` | the key verifies signatures of CRLs |
| `encipher_only` | with `key_agreement`: the agreed key only encrypts |
| `decipher_only` | with `key_agreement`: the agreed key only decrypts |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    auto bits = root.key_usage();
    auto sign = crypto::x509::key_usage::cert_sign;
    println("{}", (bits & sign) == sign);
    println("{}", root.allows(sign | crypto::x509::key_usage::crl_sign));
}
```

Output:

```text
true
true
```

## See also

- [certificate::key_usage](x509-certificate/key_usage.md), [certificate::allows](x509-certificate/allows.md)
- [ext_key_usage](x509-ext_key_usage.md): the extended key usages
- [sgcl::crypto::x509](x509.md)
