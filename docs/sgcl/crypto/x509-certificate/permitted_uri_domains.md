[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::permitted_uri_domains, excluded_uri_domains

```cpp
const vector<string>& permitted_uri_domains() const noexcept;    // (1)
const vector<string>& excluded_uri_domains() const noexcept;     // (2)
```

Return the URI subtrees of the nameConstraints:

1. the permitted ones;
2. the excluded ones.

A subtree is a domain that the host of a URI must be in (`.example.com`) or be (`example.com`). A SAN URI with no
host, or with an IP address as its host, fails under every CA with name constraints, as in Go 1.27 (RFC 5280 and
OpenSSL fail it only under a CA with a URI subtree). Directory names are not constrained: a critical extension with
such a subtree is unhandled, and the constraints do not reach the subject's distinguished name.

A CA's name constraints (RFC 5280 §4.2.1.10, checked by Go's rules) apply to every name of every certificate below
it, intermediates' too. A permitted list of a kind lets through only what one of its entries covers, an excluded one
stops what any covers, and a kind no list names passes.

## Parameters

None.

## Return value

The subtrees, in the order of the certificate; empty when there are none of the kind.

## Complexity

Constant.

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

    println("{} {}", ca.permitted_uri_domains(), ca.excluded_uri_domains());
}
```

Output:

```text
[".example.com"] ["secret.example.com"]
```

## See also

- [uris](uris.md): the URIs they apply to
- [sgcl::crypto::x509::certificate](README.md)
