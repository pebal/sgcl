[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::signature_algorithm

```cpp
x509::signature_algorithm signature_algorithm() const noexcept;
```

Returns the algorithm the response is signed with ([signature_algorithm](../x509-signature_algorithm.md)), `unknown` for one the module does not name.

## Parameters

None.

## Return value

The algorithm.

## Complexity

Constant.

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
    println("{}",
        response.signature_algorithm() == crypto::x509::signature_algorithm::ecdsa_with_sha256);
}
```

Output:

```text
true
```

## See also

- [signature_algorithm_oid](signature_algorithm_oid.md): its OID
- [sgcl::crypto::x509::ocsp_response](README.md)
