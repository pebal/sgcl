[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::unhandled_critical_extensions

```cpp
const vector<string>& unhandled_critical_extensions() const noexcept;
```

Returns the OIDs of the critical extensions the module does not handle, in the order of the certificate. A certificate
with one does not verify (`reason::unhandled_critical_extension`): its issuer said that a verifier that does not know
the extension must refuse the certificate.

## Parameters

None.

## Return value

The OIDs, empty when there are none.

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
    crypto::x509::certificate odd = crypto::x509::certificate::from_pem(odd_pem);

    println("{}", odd.unhandled_critical_extensions());
}
```

Output:

```text
["1.3.6.1.4.1.55555.1"]
```

## See also

- [extensions](extensions.md): every extension
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
