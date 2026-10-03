[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::policies

```cpp
const vector<string>& policies() const noexcept;
```

Returns the OIDs of the certificatePolicies, in the order of the certificate: `2.23.140.1.2.2` for a certificate
validated as an organization's. Policies are read, not validated: a critical policyConstraints, policyMappings or
inhibitAnyPolicy is an unhandled critical extension, so that a chain whose CA asks for policy processing fails rather
than passes unchecked (Go validates policies; the roots of the system's bundles have none of these).

## Parameters

None.

## Return value

The OIDs, empty when there is no certificatePolicies.

## Complexity

Constant.

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

    println("{}", leaf.policies());
}
```

Output:

```text
["2.23.140.1.2.2"]
```

## See also

- [unhandled_critical_extensions](unhandled_critical_extensions.md)
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
