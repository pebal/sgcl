[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::from_pem

```cpp
[[nodiscard]] static expected<certificate, error> from_pem(const string& text) noexcept;
```

Reads the first `CERTIFICATE` block of a PEM text (RFC 7468) that reads, whatever is around it: the blocks are read one
by one, as [certificate_pool::append_pem](../x509-certificate_pool/append_pem.md) reads them, and one that cannot be
read as PEM, or is of another type, is passed over, before the certificate or after it. The block's bytes are read
by [parse](parse.md).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the PEM text: a file's text or the program's |

## Return value

The certificate, or a [crypto::error](../error.md): the error of [parse](parse.md) for the block, or
`errc::malformed` for a text with no `CERTIFICATE` block that reads, whose message names the first PEM error when there
was one.

## Complexity

Linear in the length of `text`.

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
    string text = "a key, then the certificate\n" + string(leaf_pem);
    auto leaf = crypto::x509::certificate::from_pem(text);
    println("{}", leaf->subject().common_name());
    println("{}", crypto::x509::certificate::from_pem("no PEM here").error().message());
}
```

Output:

```text
www.example.com
sgcl::crypto::x509: no CERTIFICATE block in the PEM text
```

## See also

- [parse](parse.md): a certificate from its DER
- [certificate_pool::from_pem](../x509-certificate_pool/from_pem.md): every certificate of a text
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
