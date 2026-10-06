[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::check_signature_from

```cpp
expected<void, error> check_signature_from(const certificate& issuer) const noexcept;
```

Checks whether `issuer` signed the list: its subject the list's issuer, its keyUsage, where it has one, with `cRLSign`, its subject key identifier the list's authority key identifier when both have one, and the signature valid under its key (RSA PKCS #1 v1.5 and PSS, ECDSA on P-256, P-384 and P-521, Ed25519, over SHA-2; never MD5 or SHA-1). Go's `RevocationList.CheckSignatureFrom`.

## Parameters

| Parameter | Description |
|---|---|
| `issuer` | the CA whose key is checked |

## Return value

Nothing, or `errc::verification` with the reason: `unknown_authority` (another issuer, another key of it), `missing_cert_sign` (no `cRLSign`), `invalid_signature`, `unsupported_algorithm`, `insecure_algorithm`.

## Complexity

The verification of one signature, over the list's bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    auto root = crypto::x509::certificate::from_pem(io::read_text(dir + "root.pem")).value();
    println("{}", crl.check_signature_from(issuer).has_value());
    println("{}", crl.check_signature_from(root).error().message());
}
```

Output:

```text
true
sgcl::crypto::x509: the CRL of "CN=SGCL Revocation Intermediate" is not issued by "CN=SGCL Revocation Root"
```

## See also

- [status_of](status_of.md): the signature and the rest
- [sgcl::crypto::x509::revocation_list](README.md)
