[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::matches

```cpp
bool matches(const certificate& cert, const certificate& issuer) const noexcept;
```

Checks whether the request names this certificate of this issuer: its serial number the certificate's, and the
hashes of the issuer's name and key the issuer's, by the request's hash.

## Parameters

| Parameter | Description |
|---|---|
| `cert` | the certificate |
| `issuer` | its issuer |

## Return value

`true` when the CertID is the certificate's.

## Complexity

Linear in the sizes of the issuer's name and key.

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
    auto revoked = crypto::x509::certificate::from_pem(io::read_text(dir + "revoked.pem")).value();
    auto request = crypto::x509::ocsp_request::make(leaf, issuer).value();
    println("{} {}", request.matches(leaf, issuer), request.matches(revoked, issuer));
}
```

Output:

```text
true false
```

## See also

- [parse](parse.md): a request read
- [sgcl::crypto::x509::ocsp_request](README.md)
