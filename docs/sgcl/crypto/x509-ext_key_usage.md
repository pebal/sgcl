[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::ext_key_usage

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class ext_key_usage : uint8_t {
        any = 1,
        server_auth,
        client_auth,
        code_signing,
        email_protection,
        ipsec_end_system,
        ipsec_tunnel,
        ipsec_user,
        time_stamping,
        ocsp_signing,
        microsoft_server_gated_crypto,
        netscape_server_gated_crypto,
        microsoft_commercial_code_signing,
        microsoft_kernel_code_signing
    };
}
```

The purposes of a certificate's extKeyUsage (RFC 5280 §4.2.1.12) that the module knows by name: Go's `ExtKeyUsage`,
all fourteen. Any other is kept as its OID ([unknown_ext_key_usages](x509-certificate/unknown_ext_key_usages.md)). A
verification asks for some of them in [verify_options](x509-verify_options.md), `server_auth` when it names none.

| Value | Description |
|---|---|
| `any` | anyExtendedKeyUsage: every purpose; in the options, no check |
| `server_auth` | a TLS server |
| `client_auth` | a TLS client |
| `code_signing` | signed code |
| `email_protection` | S/MIME |
| `ipsec_end_system` | an IPsec end system |
| `ipsec_tunnel` | an IPsec tunnel |
| `ipsec_user` | an IPsec user |
| `time_stamping` | a time-stamping authority |
| `ocsp_signing` | an OCSP responder |
| `microsoft_server_gated_crypto` | Microsoft's Server Gated Crypto |
| `netscape_server_gated_crypto` | Netscape's Server Gated Crypto |
| `microsoft_commercial_code_signing` | Microsoft's commercial code signing |
| `microsoft_kernel_code_signing` | Microsoft's kernel code signing |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    for (auto u : cert.ext_key_usages()) {
        println("{}", u == crypto::x509::ext_key_usage::server_auth);
    }
}
```

Output:

```text
true
```

## See also

- [certificate::ext_key_usages](x509-certificate/ext_key_usages.md)
- [verify_options](x509-verify_options.md): `key_usages`
- [sgcl::crypto::x509](x509.md)
