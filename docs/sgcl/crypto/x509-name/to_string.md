[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [name](../x509-name.md)

# sgcl::crypto::x509::name::to_string

```cpp
string to_string() const noexcept;
```

Writes the name as Go's `pkix.Name.String()` writes it, roughly RFC 2253: `CN=www.example.com,O=Example\, Inc.,C=US`.
The common attributes stand in the fixed order SERIALNUMBER, CN, OU, O, POSTALCODE, STREET, L, ST, C (the values of
one type joined by `+`, in reverse of the encoding's order), then every other attribute as its dotted OID, in reverse
of the encoding's order; `,`, `+`, `"`, `\`, `<`, `>`, `;`, a leading `#` and a leading or trailing space are escaped
with `\`.

## Parameters

None.

## Return value

The text of the name, empty for an empty name.

## Complexity

Linear in the length of the attributes.

## Exceptions

None.

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

    println("{}", ca.subject().to_string());
    println("{}", ca.issuer().to_string());
}
```

Output:

```text
SERIALNUMBER=42,CN=Example Docs CA,OU=Docs,O=Example\, Inc.,POSTALCODE=00-001,STREET=Prosta 1,L=Warszawa,ST=Mazowieckie,C=PL
CN=sgcl test CA
```

## See also

- [attributes](attributes.md): the attributes in the order of the encoding
- [sgcl::crypto::x509::name](../x509-name.md)
