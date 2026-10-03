[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::verify_hostname

```cpp
[[nodiscard]] expected<void, error> verify_hostname(const string& host) const noexcept;
```

Checks that the certificate is for the DNS name `host`, by RFC 6125 §6.4 as Go matches it: against the
[dns_names](dns_names.md) only — **the common name never**, as Go since 1.15 and browsers; ASCII case folded; a
trailing `.` of `host` ignored; a wildcard only as the **whole leftmost label** (`*.example.com`), matching exactly one
label: not `example.com`, not `a.b.example.com`, and `f*.example.com` is no wildcard at all. A name written as an IPv4
or IPv6 address is refused (RFC 6125 Appendix B.2): an address is verified by its bytes, with
[verify_ip](verify_ip.md).

`host` is compared in ASCII: an internationalized name is asked in A-labels (`xn--…`), which
[txt::idna](../../txt/idna/README.md)'s `to_ascii` gives.

## Parameters

| Parameter | Description |
|---|---|
| `host` | the DNS name the certificate must be for |

## Return value

Nothing, or a [crypto::error](../error/README.md) `errc::verification`, `reason::hostname_mismatch`, whose message names the
names the certificate is for.

## Complexity

Linear in the length of the names.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

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

    for (const char* host : {"www.example.com", "WWW.EXAMPLE.COM.", "shop.api.example.com",
                             "a.b.api.example.com", "example.com"}) {
        println("{}: {}", host, leaf.verify_hostname(host).has_value());
    }
    println("{}", leaf.verify_hostname("example.com").error().message());
}
```

Output:

```text
www.example.com: true
WWW.EXAMPLE.COM.: true
shop.api.example.com: true
a.b.api.example.com: false
example.com: false
sgcl::crypto::x509: the certificate is valid for www.example.com, *.api.example.com, not example.com
```

## See also

- [verify_options](../x509-verify_options.md): `dns_name`, the same check in a verification
- [verify_ip](verify_ip.md): an address
- [sgcl::crypto::x509::certificate](README.md)
