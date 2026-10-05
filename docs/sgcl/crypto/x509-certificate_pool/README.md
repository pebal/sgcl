[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md)

# sgcl::crypto::x509::certificate_pool

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    class certificate_pool;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::x509::certificate_pool` is a set of [certificates](../x509-certificate/README.md): the roots a chain must end in,
or the intermediates it may pass through, as [verify_options](../x509-verify_options.md) takes them. Go's
`x509.CertPool`: the system's roots ([system](system.md)), the certificates of a PEM text or
file, or the program's own, one by one. A certificate is in a pool once, and the pool finds the parents of a
certificate by the bytes of its issuer's name.

## Rules

- **A handle**, as Go's `*CertPool`: a copy shares the certificates, and [add](add.md) through one is seen through every
  copy; [clone](clone.md) makes a pool of its own.
- **Reading from many threads at once is safe**; adding while another thread verifies is not.
- **Once.** A certificate of the same bytes as one in the pool is not added again: the pool keeps the SHA-256 of
  each certificate's bytes, and its certificates by their subject's name, which is what a chain is built by.
- **What does not read is passed over.** The readers of PEM add every `CERTIFICATE` block that parses and pass over
  the rest, as Go's `AppendCertsFromPEM`; a file that cannot be read is an `io::error`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](x509-certificate_pool.md) | an empty pool |

#### Reading

| Function | Description |
|---|---|
| [system, async_system](system.md) | the system's roots (static) |
| [from_pem](from_pem.md) | the certificates of a PEM text (static) |
| [from_file, async_from_file](from_file.md) | the certificates of a PEM file (static) |

#### Modifiers

| Function | Description |
|---|---|
| [add](add.md) | adds a certificate |
| [append_pem](append_pem.md) | adds the certificates of a PEM text |
| [clone](clone.md) | a pool of its own with the same certificates |

#### Lookup

| Function | Description |
|---|---|
| [size](size.md) | the number of certificates |
| [empty](empty.md) | checks whether the pool has none |
| [certificates](certificates.md) | the certificates in the order they were added |
| [contains](contains.md) | checks whether the pool has a certificate |

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
    // the program's own roots, from a file
    auto roots = crypto::x509::certificate_pool::from_file("tests/net/tls_testdata/ca.pem");
    if (!roots) {
        eprintln(roots.error().message());
        return 1;
    }
    crypto::x509::certificate_pool intermediates = crypto::x509::certificate_pool::from_pem(ca_pem);
    println("{} root, {} intermediate", roots->size(), intermediates.size());

    crypto::x509::certificate leaf = crypto::x509::certificate::from_pem(leaf_pem);
    auto at = time::datetime::from_unix(1800000000, time::zone::utc());  // 2027-01-15
    auto chain = leaf.verify({.roots = roots.value(), .intermediates = intermediates, .time = at});
    println("{} certificates", chain->size());
}
```

Output:

```text
1 root, 1 intermediate
3 certificates
```

## See also

- [certificate](../x509-certificate/README.md)
- [verify_options](../x509-verify_options.md): where the pools go
- [sgcl::crypto::x509](../x509.md)
