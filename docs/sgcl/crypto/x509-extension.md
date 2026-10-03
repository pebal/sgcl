[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::extension

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct extension {
        string oid;
        bool critical = false;
        vector<byte> value;
    };
}
```

`sgcl::crypto::x509::extension` is an extension as it is in the certificate: its OID, whether it is critical, and the
bytes of its extnValue, Go's `pkix.Extension`. The extensions the module reads are also in fields of the
[certificate](x509-certificate.md); one it does not read is here for the program to read.

## Rules

- A struct of a [string](../core/string.md) and a [vector](../core/vector.md): it lives where a `tracked_ptr` may.

## Member objects

| Member | Description |
|---|---|
| `oid` | the OID of the extension, dotted: `"2.5.29.17"` for subjectAltName |
| `critical` | whether the extension is critical; `false` by default |
| `value` | the bytes of the extnValue: the DER of the extension's own structure |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

// a leaf with a critical extension of its own, issued by the CA of ca_pem
const char* odd_pem = R"(-----BEGIN CERTIFICATE-----
MIIB9DCCAaagAwIBAgICIAQwBQYDK2VwMIGoMQswCQYDVQQGEwJQTDEUMBIGA1UE
CAwLTWF6b3dpZWNraWUxETAPBgNVBAcMCFdhcnN6YXdhMREwDwYDVQQJDAhQcm9z
dGEgMTEPMA0GA1UEEQwGMDAtMDAxMRYwFAYDVQQKDA1FeGFtcGxlLCBJbmMuMQ0w
CwYDVQQLDAREb2NzMQswCQYDVQQFEwI0MjEYMBYGA1UEAwwPRXhhbXBsZSBEb2Nz
IENBMB4XDTI2MDEwMTAwMDAwMFoXDTM2MDEwMTAwMDAwMFowGjEYMBYGA1UEAwwP
b2RkLmV4YW1wbGUuY29tMCowBQYDK2VwAyEAIJGj6FBPJvnqS0Al5xSlzm8UX0rn
mKMPaHN0CDgmitOjgYAwfjAMBgNVHRMBAf8EAjAAMBoGA1UdEQQTMBGCD29kZC5l
eGFtcGxlLmNvbTASBgkrBgEEAYOyAwEBAf8EAgUAMB0GA1UdDgQWBBR3udseVOSt
wedY9cifSaHY/vLDtjAfBgNVHSMEGDAWgBQfD1WKbZt/hpSVb+K97uksyngRUjAF
BgMrZXADQQAW7dMDQY1KGC7yMUvUPW81E2clipnHfpN2muJLOUMfh2s32lkcY6X4
m2dyhfXrcDTXdu7pOmyN+w/0Q3Es6v0F
-----END CERTIFICATE-----
)";

int main() {
    crypto::x509::certificate odd = crypto::x509::certificate::from_pem(odd_pem);

    for (auto& e : odd.extensions()) {
        if (e.critical) {
            println("{}: {}", e.oid, encoding::hex::encode(e.value));
        }
    }
}
```

Output:

```text
2.5.29.19: 3000
1.3.6.1.4.1.55555.1: 0500
```

## See also

- [certificate::extensions](x509-certificate/extensions.md)
- [certificate::unhandled_critical_extensions](x509-certificate/unhandled_critical_extensions.md)
- [sgcl::crypto::x509](x509.md)
