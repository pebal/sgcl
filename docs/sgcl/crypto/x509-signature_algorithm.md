[sgcl](../README.md) › [crypto](README.md) › [x509](x509.md)

# sgcl::crypto::x509::signature_algorithm

```cpp
#include "sgcl/crypto/x509.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x509 {
    enum class signature_algorithm : uint8_t {
        unknown = 0,
        md2_with_rsa,
        md5_with_rsa,
        sha1_with_rsa,
        sha256_with_rsa,
        sha384_with_rsa,
        sha512_with_rsa,
        sha256_with_rsa_pss,
        sha384_with_rsa_pss,
        sha512_with_rsa_pss,
        ecdsa_with_sha1,
        ecdsa_with_sha256,
        ecdsa_with_sha384,
        ecdsa_with_sha512,
        ed25519
    };
}
```

The algorithm a certificate is signed with: Go's `SignatureAlgorithm` without DSA. Those over MD2, MD5 and SHA-1 are
named so that they can be refused by name; the module verifies the RSA, RSA-PSS and ECDSA ones over SHA-256, SHA-384
and SHA-512, and Ed25519.

| Value | Description |
|---|---|
| `unknown` | an algorithm the module does not name; its OID is the certificate's `signature_algorithm_oid()` |
| `md2_with_rsa` | RSA PKCS #1 v1.5 over MD2: never accepted |
| `md5_with_rsa` | RSA PKCS #1 v1.5 over MD5: never accepted |
| `sha1_with_rsa` | RSA PKCS #1 v1.5 over SHA-1: never accepted |
| `sha256_with_rsa` | RSA PKCS #1 v1.5 over SHA-256 |
| `sha384_with_rsa` | RSA PKCS #1 v1.5 over SHA-384 |
| `sha512_with_rsa` | RSA PKCS #1 v1.5 over SHA-512 |
| `sha256_with_rsa_pss` | RSA PSS over SHA-256, MGF1 over SHA-256, a salt of 32 bytes |
| `sha384_with_rsa_pss` | RSA PSS over SHA-384, MGF1 over SHA-384, a salt of 48 bytes |
| `sha512_with_rsa_pss` | RSA PSS over SHA-512, MGF1 over SHA-512, a salt of 64 bytes |
| `ecdsa_with_sha1` | ECDSA over SHA-1: never accepted |
| `ecdsa_with_sha256` | ECDSA over SHA-256, on P-256 or P-384 |
| `ecdsa_with_sha384` | ECDSA over SHA-384, on P-256 or P-384 |
| `ecdsa_with_sha512` | ECDSA over SHA-512, on P-256 or P-384 |
| `ed25519` | Ed25519 |

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
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    crypto::x509::certificate leaf = crypto::x509::certificate::from_pem(leaf_pem);

    auto algorithm = cert.signature_algorithm();
    println("{}", algorithm == crypto::x509::signature_algorithm::ecdsa_with_sha256);
    println("{}", leaf.signature_algorithm() == crypto::x509::signature_algorithm::ed25519);
}
```

Output:

```text
true
true
```

## See also

- [certificate::signature_algorithm](x509-certificate/signature_algorithm.md)
- [reason](x509-reason.md): `insecure_algorithm`, `unsupported_algorithm`
- [sgcl::crypto::x509](x509.md)
