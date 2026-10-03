[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](README.md)

# sgcl::crypto::x509::certificate_pool::append_pem

```cpp
size_t append_pem(const string& text) noexcept;
```

Adds every `CERTIFICATE` block of a PEM text that parses, as Go's `AppendCertsFromPEM`: blocks of other types, blocks
with RFC 1421 headers and certificates that do not parse are passed over. After a block that does not read as PEM,
the text is read on from the line after its `BEGIN`, so that a `BEGIN` without an `END` of its own does not take the
next block with it. A certificate the pool has already is not added again, and is counted.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text |

## Return value

How many certificates parsed; Go's `AppendCertsFromPEM` answers `true` when one did.

## Complexity

Linear in the length of the text.

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
    crypto::x509::certificate_pool pool;
    string text = "-----BEGIN CERTIFICATE-----\nbroken\n" + string(ca_pem) + string(ca_pem);
    println("{} parsed, {} in the pool", pool.append_pem(text), pool.size());
}
```

Output:

```text
2 parsed, 1 in the pool
```

## See also

- [from_pem](from_pem.md): a new pool of a text
- [add](add.md)
- [sgcl::crypto::x509::certificate_pool](README.md)
