[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::ip_range

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    struct ip_range {
        ip_address address;
        ip_address mask;
    };
}
```

`sgcl::crypto::x509::ip_range` is an IP range of a CA's name constraints: an [address](x509-ip_address.md) and a mask
of the same size, the mask ones then zeros (`10.0.0.0/255.0.0.0`), as the certificate holds them.

## Rules

- A plain value of two addresses: it lives anywhere and holds nothing.

## Member objects

| Member | Description |
|---|---|
| `address` | the address of the range |
| `mask` | the mask, of the size of `address` |

## Member functions

| Function | Description |
|---|---|
| [contains](x509-ip_range/contains.md) | checks whether an address is in the range |

## Example

```cpp
#include "sgcl/core.h"
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

    auto bytes = [](const crypto::x509::ip_address& a) {
        vector<int> out;
        for (int i : range(int(a.size))) {
            out.push_back(int(a.bytes[i]));
        }
        return out;
    };
    for (auto& r : ca.permitted_ip_ranges()) {
        println("{} / {}", bytes(r.address), bytes(r.mask));
    }
}
```

Output:

```text
[10, 0, 0, 0] / [255, 0, 0, 0]
```

## See also

- [certificate::permitted_ip_ranges](x509-certificate/permitted_ip_ranges.md)
- [sgcl::crypto::x509](x509.md)
