[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::signature_algorithm_oid

```cpp
const string& signature_algorithm_oid() const noexcept;
```

Returns the OID of the response's signature algorithm, as text.

## Parameters

None.

## Return value

The OID, `"1.2.840.10045.4.3.2"`.

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
    println("{}", response.signature_algorithm_oid());
}
```

Output:

```text
1.2.840.10045.4.3.2
```

## See also

- [signature_algorithm](signature_algorithm.md): the algorithm by name
- [sgcl::crypto::x509::ocsp_response](README.md)
