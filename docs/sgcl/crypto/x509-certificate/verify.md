[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::verify

```cpp
/*(1)*/ [[nodiscard]] expected<x509::chain, error> verify() const;
/*(2)*/ [[nodiscard]] expected<x509::chain, error> verify(const verify_options& options) const;
```

Builds and checks a chain from this certificate to a trusted root, Go's `Verify`.

1. The system's roots ([certificate_pool::system](../x509-certificate_pool/system.md)), the time now, the usage
   `server_auth`, no name.
2. As the [options](../x509-verify_options.md) ask: the roots, the intermediates, the DNS name or the IP address the
   certificate must be for, the time, the extended key usages.

The chain runs from the certificate through the intermediates to a certificate of the roots: the parents a pool has
for the issuer's name (its [raw_issuer](raw_issuer.md)), roots before intermediates, depth first, the key identifiers
only ordering them. **At most ten intermediates**, no certificate twice (a loop of cross-signatures ends), at most a
hundred signatures tried (Go's budget: on a hostile pool of RSA-16384 keys about a second).

Each signature is checked: RSA PKCS #1 v1.5 and PSS (MGF1 over the same hash, the salt as long as the hash — the three
sets of parameters Go takes — and the signature's salt exactly that long, as Go and OpenSSL check it), ECDSA on P-256
and P-384, Ed25519; over SHA-256, SHA-384 and SHA-512. **MD5 and SHA-1 are never accepted**
(`insecure_algorithm`), as Go has them since 1.18. A root's own signature is never checked: any certificate of the
roots ends a chain, trusted as it is. Each certificate is valid at the time asked; each issuer is a CA (a version 3
certificate needs basicConstraints with cA, its keyUsage, where there is one, keyCertSign); each pathLenConstraint is
kept; no critical extension is left unhandled; the name constraints of every CA hold over every name below it; the
extended key usages nest down the chain: walking down from the root, a certificate that lists usages (and not `any`)
crosses out every usage asked it does not list, and the chain passes while one is left. The leaf is for the name or
the address asked, as [verify_hostname](verify_hostname.md) and [verify_ip](verify_ip.md) check them.

**Revocation is not checked**: no CRL, no OCSP, no certificate transparency; a revoked certificate verifies until it
expires. **Policies are not validated** ([policies](policies.md)).

## Parameters

| Parameter | Description |
|---|---|
| `options` | the roots, the intermediates, the name, the time and the usages |

## Return value

The first chain that passes, leaf first and root last; or a [crypto::error](../error.md) `errc::verification` with its
[reason](../x509-reason.md) and the certificate at fault in its message; or `errc::unsupported` when the roots are the
system's and the system has none.

When no chain is found, the reason is, at each level, the one of the first parent whose signature over the child
verified, as deep as it went (an expired intermediate is `expired`, not `unknown_authority`); only when no parent's
signature verified, the first of those failures (`invalid_signature`, `not_a_ca`, `missing_cert_sign`, an
algorithm); `unknown_authority` when the pools hold no parent of the issuer's name. So a root rolled over to a new key,
the old one in the pool and tried first, does not hide why the path through the new key failed. A chain that reached
a root but broke a name constraint or a key usage says so.

## Complexity

That of the signatures checked, at most a hundred, and of the name constraints, at most a million comparisons
(`reason::too_many_constraints` past them).

## Exceptions

None of its own: a certificate, however broken, is the error returned. Without roots of its own, the first call
reads the system's roots, as [certificate_pool::system](../x509-certificate_pool/system.md) does.

## Notes

Compare `reason()`, not errors: [error](../error.md)'s `==` compares the message too, so `r.error() ==
error(reason::expired, "")` is never true.

Where this verification and OpenSSL's answer differently, it is mostly Go's answer (the tests hold both):

- The last second of `not_after` is valid (OpenSSL ends the period there).
- `anyExtendedKeyUsage` in a CA allows every usage (OpenSSL's `ssl_server` purpose wants `serverAuth` there).
- An issuer whose first path fails is tried again through another (OpenSSL takes the first issuer it finds).
- A wildcard DNS name that could match an excluded name is excluded.
- The key identifiers are a hint, not a requirement, as in Go: a parent whose subjectKeyIdentifier differs from the
  child's authorityKeyIdentifier is still tried (OpenSSL does not take it as the issuer, and says it found none).
- A self-issued intermediate does not count against a pathLenConstraint, as RFC 5280 and OpenSSL have it; Go counts
  it.
- A version 3 root without basicConstraints cannot sign, even with a keyUsage of keyCertSign: `not_a_ca`, as in Go
  (OpenSSL takes it as a trust anchor).
- A trust anchor need not be self-signed: any certificate of the roots ends a chain, its own signature never checked
  (OpenSSL wants `X509_V_FLAG_PARTIAL_CHAIN` for one that is not).
- A `dNSName` of a certificate with a trailing dot is read and matched only exactly, as Go matches it; under a CA with
  name constraints it cannot be checked, and fails.
- A SAN URI with no host, or with an IP address as its host, fails under every CA with name constraints, as in Go 1.27
  (RFC 5280 and OpenSSL fail it only under a CA with a URI subtree).
- An IPv4-mapped address in a certificate's SAN is its IPv4 address, IPv4 ranges of the name constraints applying to
  it (Go keeps the 16 bytes and compares them with IPv6 ranges).
- A host asked with a trailing dot (`example.com.`) is the name without it, as in Go.

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

int main() {
    crypto::x509::certificate leaf = crypto::x509::certificate::from_pem(leaf_pem);

    crypto::x509::certificate ca = crypto::x509::certificate::from_pem(ca_pem);

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    auto at = time::datetime::from_unix(1800000000, time::zone::utc());  // 2027-01-15

    crypto::x509::certificate_pool roots;
    roots.add(root);
    crypto::x509::certificate_pool intermediates;
    intermediates.add(ca);
    auto chain = leaf.verify({.roots = roots, .intermediates = intermediates,
                              .dns_name = "shop.api.example.com", .time = at});
    for (auto& c : chain.value()) {
        println("{}", c.subject().common_name());
    }

    auto later = time::datetime::from_unix(2200000000, time::zone::utc());  // 2039
    auto expired = leaf.verify({.roots = roots, .intermediates = intermediates, .time = later});
    println("{}", expired.error().reason() == crypto::x509::reason::expired);
    auto alone = leaf.verify({.roots = roots, .time = at});
    println("{}", alone.error().message());
}
```

Output:

```text
www.example.com
Example Docs CA
sgcl test CA
true
sgcl::crypto::x509: the certificate "CN=www.example.com,O=Example\, Inc." is signed by an unknown authority
```

The system's roots, at the time now:

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

    auto r = leaf.verify({.dns_name = "www.example.com"});
    println("{}", r ? string("trusted") : r.error().message());
}
```

## See also

- [verify_options](../x509-verify_options.md): what the second form takes
- [reason](../x509-reason.md): why a chain does not verify
- [certificate_pool](../x509-certificate_pool.md): the roots and the intermediates
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
