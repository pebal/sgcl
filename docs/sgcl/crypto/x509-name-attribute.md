[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md) › [name](x509-name.md)

# sgcl::crypto::x509::name::attribute

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class name {
    public:
        struct attribute {
            string oid;
            string value;
            bool text = false;
        };
    };
}
```

`sgcl::crypto::x509::name::attribute` is one attribute of a distinguished [name](x509-name.md): its type as a dotted
OID (`"2.5.4.3"` for CN) and its value, as text when it is one of the six string types of RFC 5280, else `#` and the
hex of its DER (RFC 4514 §2.4).

## Rules

- A struct of two [strings](../core/string.md) and a flag: it lives where a `tracked_ptr` may.

## Member objects

| Member | Description |
|---|---|
| `oid` | the type of the attribute, a dotted OID: `"2.5.4.3"` (CN), `"2.5.4.10"` (O), … |
| `value` | the value: its text in UTF-8 when `text`, else `#` and the hex of its DER |
| `text` | whether the value is of a string type, read as text; `false` by default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

// a CA with name constraints, issued by the tree's test CA
const char* ca_pem = R"(-----BEGIN CERTIFICATE-----
MIICnDCCAkGgAwIBAgICEAEwCgYIKoZIzj0EAwIwFzEVMBMGA1UEAwwMc2djbCB0
ZXN0IENBMB4XDTI2MDEwMTAwMDAwMFoXDTQ2MDEwMTAwMDAwMFowgagxCzAJBgNV
BAYTAlBMMRQwEgYDVQQIDAtNYXpvd2llY2tpZTERMA8GA1UEBwwIV2Fyc3phd2Ex
ETAPBgNVBAkMCFByb3N0YSAxMQ8wDQYDVQQRDAYwMC0wMDExFjAUBgNVBAoMDUV4
YW1wbGUsIEluYy4xDTALBgNVBAsMBERvY3MxCzAJBgNVBAUTAjQyMRgwFgYDVQQD
DA9FeGFtcGxlIERvY3MgQ0EwKjAFBgMrZXADIQB9rx1X8uCF/nApxoz2hsDLgz4i
+FcJdyJ+hTOEBpfWNaOCARgwggEUMBIGA1UdEwEB/wQIMAYBAf8CAQAwDgYDVR0P
AQH/BAQDAgEGMIGYBgNVHR4BAf8EgY0wgYqgOjANggtleGFtcGxlLmNvbTAKhwgK
AAAA/wAAADANgQtleGFtcGxlLmNvbTAOhgwuZXhhbXBsZS5jb22hTDAUghJzZWNy
ZXQuZXhhbXBsZS5jb20wCocICgkAAP//AAAwEoEQYm9zc0BleGFtcGxlLmNvbTAU
hhJzZWNyZXQuZXhhbXBsZS5jb20wEwYDVR0gBAwwCjAIBgZngQwBAgIwHQYDVR0O
BBYEFB8PVYptm3+GlJVv4r3u6SzKeBFSMB8GA1UdIwQYMBaAFKCaIw2saHcTbsTD
6EYMC34TuF8RMAoGCCqGSM49BAMCA0kAMEYCIQCR0VoikrXXxo8rNKytUP4iXQbD
JWlNLDxoB4FyfALj9QIhAKEUOq8FQzB3eNBQRdmC8hG/O8RZUMz2abpAQkQTjEtG
-----END CERTIFICATE-----
)";

int main() {
    crypto::x509::certificate ca = crypto::x509::certificate::from_pem(ca_pem);

    auto& first = ca.subject().attributes()[0];
    println("{} {} {}", first.oid, first.value, first.text);
}
```

Output:

```text
2.5.4.6 PL true
```

## See also

- [name::attributes](x509-name/attributes.md)
- [sgcl::crypto::x509::name](x509-name.md)
