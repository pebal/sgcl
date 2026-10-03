[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](README.md)

# sgcl::crypto::x509::certificate::has_key_usage

```cpp
bool has_key_usage() const noexcept;
```

Checks whether the certificate has a keyUsage extension. A CA without one may sign certificates; one with one needs
its keyCertSign bit.

## Parameters

None.

## Return value

`true` when the certificate has a keyUsage, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

// a leaf with a critical extension of its own, issued by the CA of ca_pem
const char* odd_pem = R"(-----BEGIN CERTIFICATE-----
MIIB9DCCAaagAwIBAgICIAQwBQYDK2VwMIGoMQswCQYDVQQGEwJQTDEUMBIGA1UE
CAwLTWF6b3dpZWNraWUxETAPBgNVBAcMCFdhcnN6YXdhMREwDwYDVQQJDAhQcm9z
dGEgMTEPMA0GA1UEEQwGMDAtMDAxMRYwFAYDVQQKDA1FeGFtcGxlLCBJbmMuMQ0w
CwYDVQQLDAREb2NzMQswCQYDVQQFEwI0MjEYMBYGA1UEAwwPRXhhbXBsZSBEb2Nz
IENBMB4XDTI2MDEwMTAwMDAwMFoXDTM2MDEwMTAwMDAwMFowGjEYMBYGA1UEAwwP
b2RkLmV4YW1wbGUuY29tMCowBQYDK2VwAyEAIJGj6FBPJvnqS0Al5xSlzm8UX0rn
mKMPaHN0CDgmitOjgYAwfjAMBgNVHRMBAf8EAjAAMBoGA1UdEQQTMBGCD29kZC5l
eGFtcGxlLmNvbTASBgkrBgEEAYOyAwEBAf8EAgUAMB0GA1UdDgQWBBR3udseVOSt
wedY9cifSaHY/vLDtjAfBgNVHSMEGDAWgBQfD1WKbZt/hpSVb+K97uksyngRUjAF
BgMrZXADQQAW7dMDQY1KGC7yMUvUPW81E2clipnHfpN2muJLOUMfh2s32lkcY6X4
m2dyhfXrcDTXdu7pOmyN+w/0Q3Es6v0F
-----END CERTIFICATE-----
)";

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    crypto::x509::certificate odd = crypto::x509::certificate::from_pem(odd_pem);

    println("{} {}", cert.has_key_usage(), odd.has_key_usage());
}
```

Output:

```text
true false
```

## See also

- [key_usage](key_usage.md), [allows](allows.md): its bits
- [sgcl::crypto::x509::certificate](README.md)
