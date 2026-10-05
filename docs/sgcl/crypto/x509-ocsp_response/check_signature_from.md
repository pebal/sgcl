[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::check_signature_from

```cpp
expected<void, error> check_signature_from(const certificate& signer) const noexcept;
```

Checks whether `signer`'s key signed the response: the algorithm one the module verifies (RSA PKCS #1 v1.5 and PSS, ECDSA on P-256 and P-384, Ed25519, over SHA-2; never MD5 or SHA-1), the signature valid under the key. Nothing else is checked: whether `signer` may sign for a certificate is [verify](verify.md)'s question. Go's `Response.CheckSignatureFrom`.

## Parameters

| Parameter | Description |
|---|---|
| `signer` | the certificate whose key is checked |

## Return value

Nothing, or `errc::verification` with the reason: `invalid_signature`, `unsupported_algorithm`, `insecure_algorithm`; `revocation_unknown` for a response that is not successful, which has no signature.

## Complexity

The verification of one signature.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto response = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good.der").value()).value();
    auto responder = crypto::x509::certificate::from_pem(
        io::read_text(dir + "responder.pem")).value();
    println("{}", response.check_signature_from(responder).has_value());
    println("{}", response.check_signature_from(issuer).error().message());
}
```

Output:

```text
true
sgcl::crypto::x509: the signature of the OCSP response does not verify under the key of "CN=SGCL Revocation Intermediate"
```

## See also

- [verify](verify.md): the signer, the certificate and the times
- [sgcl::crypto::x509::ocsp_response](README.md)
