[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::name

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class name;
}
```

`sgcl::crypto::x509::name` is a distinguished name (RFC 5280 §4.1.2.4, X.501): the issuer or the subject of a
[certificate](../x509-certificate/README.md). It is its attributes in the order of the encoding, each a type (an OID) and a
value, with the common ones by name, as Go's `pkix.Name` has them: [common_name](common_name.md) the last
CN, [country](country.md) every C, and so on; and the text Go's `pkix.Name.String()` writes.

The values of the six string types of RFC 5280 are read into UTF-8 as Go's x509 reads them (PrintableString with `*`
and `&`, which real certificates hold; T61String as Latin-1; BMPString as UCS-2, its surrogates and noncharacters
refused; UTF8String, IA5String and NumericString checked); a value of another type is kept as its DER, written as `#`
and its hex, as RFC 4514 §2.4 has it.

## Rules

- **A value**, read from a certificate and never changed. It holds a [vector](../../core/vector/README.md) of
  [attributes](../x509-name-attribute.md), so it lives where a `tracked_ptr` may.
- **Text is for reading, bytes are for chains.** A chain is built on the bytes of the names
  ([raw_issuer](../x509-certificate/raw_issuer.md), [raw_subject](../x509-certificate/raw_subject.md)); a name compares its
  attributes as text.
- **At most 64 attributes** in a name: real names have under a dozen.

## Member types

| Type | Definition |
|---|---|
| [attribute](../x509-name-attribute.md) | one attribute: its OID, its value, whether the value is text |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](x509-name.md) | an empty name |

#### Attributes

| Function | Description |
|---|---|
| [attributes](attributes.md) | every attribute, in the order of the encoding |
| [empty](empty.md) | checks whether the name has none |
| [common_name](common_name.md) | the last CN |
| [serial_number](serial_number.md) | the last SERIALNUMBER |
| [country](country.md) | every C |
| [organization](organization.md) | every O |
| [organizational_unit](organizational_unit.md) | every OU |
| [locality](locality.md) | every L |
| [province](province.md) | every ST |
| [street_address](street_address.md) | every STREET |
| [postal_code](postal_code.md) | every POSTALCODE |

#### Conversions

| Function | Description |
|---|---|
| [to_string](to_string.md) | the text Go's `pkix.Name.String()` writes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the attributes |

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

    auto& subject = ca.subject();
    println("{}", subject.to_string());
    println("{} of {}, {}", subject.common_name(), subject.organization(), subject.country());
}
```

Output:

```text
SERIALNUMBER=42,CN=Example Docs CA,OU=Docs,O=Example\, Inc.,POSTALCODE=00-001,STREET=Prosta 1,L=Warszawa,ST=Mazowieckie,C=PL
Example Docs CA of ["Example, Inc."], ["PL"]
```

## See also

- [certificate::subject](../x509-certificate/subject.md), [certificate::issuer](../x509-certificate/issuer.md)
- [sgcl::crypto::x509](../x509.md)
