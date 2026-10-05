[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::certificate_template

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct certificate_template {
        vector<byte> serial_number;
        string common_name;
        vector<string> organization;
        optional<time::datetime> not_before;
        optional<time::datetime> not_after;
        vector<string> dns_names;
        vector<x509::ip_address> ip_addresses;
        vector<string> email_addresses;
        vector<string> uris;
        bool is_ca = false;
        optional<int64_t> max_path_length;
        optional<x509::key_usage> key_usage;
        vector<ext_key_usage> ext_key_usages;
        vector<extension> extensions;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

The fields of a certificate to make, what [create_certificate](x509-create_certificate.md) writes: Go's
`x509.Certificate` used as a template. Every field has a default that makes a certificate: an empty template is a
certificate with a random serial, valid from now for a year, of no name.

The subjectKeyIdentifier is always written (SHA-1 of the subject's public key bits, RFC 5280 §4.2.1.2's first
method), and the authorityKeyIdentifier when the issuer has a subjectKeyIdentifier. The subjectAltName is critical when
the subject is empty (§4.2.1.6). A field that cannot be written — a name that is not ASCII, an OID that is not one, a
serial past 20 bytes — makes [create_certificate](x509-create_certificate.md) throw `std::invalid_argument`.

## Member objects

| Object | Description |
|---|---|
| `serial_number` | the serial, big-endian and positive, at most 20 bytes (RFC 5280 §4.1.2.2); empty: 16 random bytes |
| `common_name` | the subject's CN; empty: none |
| `organization` | the subject's O, one attribute each, before the CN |
| `not_before` | the start of the validity; none: now |
| `not_after` | the end of the validity; none: a year after `not_before`. UTCTime for the years 1950 to 2049, GeneralizedTime outside |
| `dns_names` | the DNS names of the subjectAltName, in ASCII (an IDN as its A-label); a wildcard as `*.example.com` |
| `ip_addresses` | the IP addresses of the subjectAltName, 4 or 16 bytes each |
| `email_addresses` | the email addresses of the subjectAltName |
| `uris` | the URIs of the subjectAltName |
| `is_ca` | basicConstraints with cA: a CA; `false` writes basicConstraints without it |
| `max_path_length` | a CA's pathLenConstraint; none: no limit |
| `key_usage` | the bits of keyUsage, critical; none: digitalSignature (and keyEncipherment for an RSA key) for a leaf, keyCertSign, cRLSign and digitalSignature for a CA |
| `ext_key_usages` | the purposes of extKeyUsage; empty: none written |
| `extensions` | more extensions, written after the module's own; one of the OID of an extension the module writes replaces it |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template t;
    t.common_name = "Example Root";
    t.organization = {"Example, Inc."};
    t.is_ca = true;
    t.max_path_length = 0;
    t.not_before = time::datetime::from_unix(1800000000, time::zone::utc());
    t.not_after = time::datetime::from_unix(2115000000, time::zone::utc());
    auto root = crypto::x509::create_certificate(t, key);
    println("{}, CA {}, path length {}", root.subject().common_name(), root.is_ca(),
            *root.max_path_length());
    println("{} to {}", root.not_before(), root.not_after());
}
```

Output:

```text
Example Root, CA true, path length 0
2027-01-15T08:00:00Z to 2037-01-08T04:00:00Z
```

## See also

- [create_certificate](x509-create_certificate.md): the certificate of a template
- [certificate_request_template](x509-certificate_request_template.md): a request's fields
- [x509](x509.md)
