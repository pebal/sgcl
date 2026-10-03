[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::reason

```cpp
#include "sgcl/crypto/error.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class reason : uint8_t {
        none = 0,
        expired,
        not_yet_valid,
        unknown_authority,
        hostname_mismatch,
        name_constraints,
        unsupported_algorithm,
        insecure_algorithm,
        invalid_signature,
        too_many_intermediates,
        path_length,
        not_a_ca,
        missing_cert_sign,
        incompatible_usage,
        unhandled_critical_extension,
        too_many_constraints
    };
}
```

Why a certificate chain does not verify: the `reason()` of a [crypto::error](error/README.md) whose code is
`errc::verification`, and `none` for every other error. Go's `x509` has these as the types and the `InvalidReason` of
its errors; here they are one list, read by a `switch`, and the certificate at fault is named in the error's message.
Each value is given with what Go and OpenSSL answer in its place.

| Value | Description |
|---|---|
| `none` | not a verification error: the system's roots missing (`errc::unsupported`), a certificate that does not parse (`errc::malformed`) |
| `expired` | a certificate of the chain is past its `not_after` at the time asked (Go's `Expired`; OpenSSL's `CERT_HAS_EXPIRED`) |
| `not_yet_valid` | a certificate of the chain is before its `not_before` (Go's `Expired`; OpenSSL's `CERT_NOT_YET_VALID`) |
| `unknown_authority` | no chain leads to a root of the pool (Go's `UnknownAuthorityError`; OpenSSL's `UNABLE_TO_GET_ISSUER_CERT_LOCALLY`) |
| `hostname_mismatch` | the leaf is not for the DNS name or the IP address asked (Go's `HostnameError`; OpenSSL's `HOSTNAME_MISMATCH`, `IP_ADDRESS_MISMATCH`) |
| `name_constraints` | a name of the chain is outside a CA's name constraints, or cannot be checked against them (Go's `CANotAuthorizedForThisName`; OpenSSL's `PERMITTED_VIOLATION`, `EXCLUDED_VIOLATION`) |
| `unsupported_algorithm` | a signature or an issuer's key of an algorithm the module does not verify (Go's `ErrUnsupportedAlgorithm`) |
| `insecure_algorithm` | a signature over MD2, MD5 or SHA-1, never accepted (Go's `InsecureAlgorithmError`; OpenSSL's `CA_MD_TOO_WEAK`) |
| `invalid_signature` | a signature that does not verify under its issuer's key (a hint of Go's `UnknownAuthorityError`; OpenSSL's `CERT_SIGNATURE_FAILURE`) |
| `too_many_intermediates` | more than ten intermediates, or more than a hundred signatures tried (OpenSSL's `CERT_CHAIN_TOO_LONG`) |
| `path_length` | a CA's pathLenConstraint is exceeded (Go's `TooManyIntermediates`; OpenSSL's `PATH_LENGTH_EXCEEDED`) |
| `not_a_ca` | an issuer without basicConstraints cA (Go's `NotAuthorizedToSign`; OpenSSL's `INVALID_CA`) |
| `missing_cert_sign` | a CA whose keyUsage lacks keyCertSign (a hint of Go's; OpenSSL's `INVALID_CA`) |
| `incompatible_usage` | no extended key usage asked is allowed by the whole chain (Go's `IncompatibleUsage`; OpenSSL's `INVALID_PURPOSE`) |
| `unhandled_critical_extension` | a critical extension the module does not know (Go's `UnhandledCriticalExtension`; OpenSSL's `UNHANDLED_CRITICAL_EXTENSION`) |
| `too_many_constraints` | name constraints that would take more than a million comparisons (Go's `TooManyConstraints`) |

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

// a leaf for www.example.org, issued by the CA of ca_pem
const char* outside_pem = R"(-----BEGIN CERTIFICATE-----
MIIB3zCCAZGgAwIBAgICIAMwBQYDK2VwMIGoMQswCQYDVQQGEwJQTDEUMBIGA1UE
CAwLTWF6b3dpZWNraWUxETAPBgNVBAcMCFdhcnN6YXdhMREwDwYDVQQJDAhQcm9z
dGEgMTEPMA0GA1UEEQwGMDAtMDAxMRYwFAYDVQQKDA1FeGFtcGxlLCBJbmMuMQ0w
CwYDVQQLDAREb2NzMQswCQYDVQQFEwI0MjEYMBYGA1UEAwwPRXhhbXBsZSBEb2Nz
IENBMB4XDTI2MDEwMTAwMDAwMFoXDTM2MDEwMTAwMDAwMFowGjEYMBYGA1UEAwwP
d3d3LmV4YW1wbGUub3JnMCowBQYDK2VwAyEACOK0QZ9l3+KUq5dE14RWTOPbltjh
tnPvgEeTSJBRpamjbDBqMAwGA1UdEwEB/wQCMAAwGgYDVR0RBBMwEYIPd3d3LmV4
YW1wbGUub3JnMB0GA1UdDgQWBBTJgnNQwGozJMEAZ1Bzimov7/OiJDAfBgNVHSME
GDAWgBQfD1WKbZt/hpSVb+K97uksyngRUjAFBgMrZXADQQCpX30Aq34I/SSOV7MP
Bu2IsbCAsHHs7xxAImkcLip/Iwkft8iYibNcyU3NxBWNxrEZxzMpdMSfaUHGNSD8
QIEG
-----END CERTIFICATE-----
)";

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
    crypto::x509::certificate ca = crypto::x509::certificate::from_pem(ca_pem);

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    auto at = time::datetime::from_unix(1800000000, time::zone::utc());  // 2027-01-15

    crypto::x509::certificate_pool roots;
    roots.add(root);
    crypto::x509::certificate_pool intermediates;
    intermediates.add(ca);
    for (const char* text : {leaf_pem, outside_pem, odd_pem}) {
        crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);
        auto r = cert.verify({.roots = roots, .intermediates = intermediates, .time = at});
        switch (r ? crypto::x509::reason::none : r.error().reason()) {
            case crypto::x509::reason::none: println("verified"); break;
            case crypto::x509::reason::name_constraints:
            println("a name the CA may not certify");
            break;
            default: println(r.error().message()); break;
        }
    }
}
```

Output:

```text
verified
a name the CA may not certify
sgcl::crypto::x509: the certificate "CN=odd.example.com" has a critical extension not handled here (1.3.6.1.4.1.55555.1)
```

## See also

- [error](error/README.md): `reason()`, and `errc::verification`
- [certificate::verify](x509-certificate/verify.md): how the reason of a failed search is chosen
- [sgcl::crypto::x509](x509.md)
