[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::signature_algorithm

```cpp
x509::signature_algorithm signature_algorithm() const noexcept;
```

Returns the algorithm the list is signed with ([signature_algorithm](../x509-signature_algorithm.md)), `unknown` for one the module does not name.

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
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    println("{}",
        crl.signature_algorithm() == crypto::x509::signature_algorithm::ecdsa_with_sha256);
}
```

Output:

```text
true
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
