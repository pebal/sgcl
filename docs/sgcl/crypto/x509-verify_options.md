[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::verify_options

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct verify_options {
        optional<certificate_pool> roots;
        certificate_pool intermediates;
        string dns_name;
        slice<const byte> ip;
        optional<time::datetime> time;
        vector<ext_key_usage> key_usages;
    };
}
```

`sgcl::crypto::x509::verify_options` is what [certificate::verify](x509-certificate/verify.md) checks against, Go's
`x509.VerifyOptions`: the roots and the intermediates of the chain, the name or the address the certificate must be
for, the time, and the extended key usages. Every field has a default, and a call names the fields it sets, in their
order: `cert.verify({.roots = pool, .dns_name = "example.com"})`.

## Rules

- The options hold [certificate_pool](x509-certificate_pool/README.md)s, so they live where a `tracked_ptr` may: on a stack,
  in a managed object, in a container of the library.
- `ip` is a view of the program's bytes, read during the call.

## Member objects

| Member | Description |
|---|---|
| `roots` | the pool a chain must end in; `nullopt`, the default: the system's ([certificate_pool::system](x509-certificate_pool/system.md), loaded once) |
| `intermediates` | the certificates a chain may pass through; none by default |
| `dns_name` | the DNS name the leaf must be for, as [verify_hostname](x509-certificate/verify_hostname.md) checks it; empty, the default: no name checked |
| `ip` | the IP address the leaf must be for, 4 or 16 bytes in network order, as [verify_ip](x509-certificate/verify_ip.md) checks it; empty, the default: no address checked |
| `time` | the instant every certificate must be valid at; `nullopt`, the default: `time::now()`, which a test's manual clock moves |
| `key_usages` | the extended key usages one of which the chain must allow; empty, the default: `server_auth`; `ext_key_usage::any`: no check |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

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

// a leaf for www.example.com, issued by the CA of ca_pem
const char* leaf_pem = R"(-----BEGIN CERTIFICATE-----
MIICkzCCAkWgAwIBAgICIAIwBQYDK2VwMIGoMQswCQYDVQQGEwJQTDEUMBIGA1UE
CAwLTWF6b3dpZWNraWUxETAPBgNVBAcMCFdhcnN6YXdhMREwDwYDVQQJDAhQcm9z
dGEgMTEPMA0GA1UEEQwGMDAtMDAxMRYwFAYDVQQKDA1FeGFtcGxlLCBJbmMuMQ0w
CwYDVQQLDAREb2NzMQswCQYDVQQFEwI0MjEYMBYGA1UEAwwPRXhhbXBsZSBEb2Nz
IENBMB4XDTI2MDEwMTAwMDAwMFoXDTM2MDEwMTAwMDAwMFowMjEWMBQGA1UECgwN
RXhhbXBsZSwgSW5jLjEYMBYGA1UEAwwPd3d3LmV4YW1wbGUuY29tMCowBQYDK2Vw
AyEACOK0QZ9l3+KUq5dE14RWTOPbltjhtnPvgEeTSJBRpamjggEGMIIBAjAMBgNV
HRMBAf8EAjAAMA4GA1UdDwEB/wQEAwIHgDApBgNVHSUEIjAgBggrBgEFBQcDAQYI
KwYBBQUHAwIGCisGAQQBgjcKAwwwYgYDVR0RBFswWYIPd3d3LmV4YW1wbGUuY29t
ghEqLmFwaS5leGFtcGxlLmNvbYERYWRtaW5AZXhhbXBsZS5jb22HBAoAAAeGGmh0
dHBzOi8vYXBwLmV4YW1wbGUuY29tL2lkMBMGA1UdIAQMMAowCAYGZ4EMAQICMB0G
A1UdDgQWBBTJgnNQwGozJMEAZ1Bzimov7/OiJDAfBgNVHSMEGDAWgBQfD1WKbZt/
hpSVb+K97uksyngRUjAFBgMrZXADQQCVajTwxc4gBqFIrntyNpf9l2W3o0pD0t+Q
a36iW1CIW/Uv9wasFFePQvIeCtkmV8PT/EgKlhaV/1uNfjgzKg4N
-----END CERTIFICATE-----
)";

int main() {
    crypto::x509::certificate leaf = crypto::x509::certificate::from_pem(leaf_pem);

    crypto::x509::certificate ca = crypto::x509::certificate::from_pem(ca_pem);

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    auto at = time::datetime::from_unix(1800000000, time::zone::utc());  // 2027-01-15

    crypto::x509::verify_options options{.roots = crypto::x509::certificate_pool(),
                                         .intermediates = crypto::x509::certificate_pool(),
                                         .time = at};
    options.roots->add(root);
    options.intermediates.add(ca);
    println("{}", leaf.verify(options).has_value());

    options.key_usages = {crypto::x509::ext_key_usage::code_signing};
    auto refused = leaf.verify(options);
    println("{}", refused.error().reason() == crypto::x509::reason::incompatible_usage);
    const unsigned char ip[] = {10, 0, 0, 7};
    options.key_usages = {crypto::x509::ext_key_usage::client_auth};
    options.ip = ip;
    println("{}", leaf.verify(options).has_value());
}
```

Output:

```text
true
true
true
```

## See also

- [certificate::verify](x509-certificate/verify.md)
- [ext_key_usage](x509-ext_key_usage.md): the usages
- [time::datetime](../time/datetime/README.md): the time
- [sgcl::crypto::x509](x509.md)
