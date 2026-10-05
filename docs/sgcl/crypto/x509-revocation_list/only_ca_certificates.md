[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::only_ca_certificates

```cpp
bool only_ca_certificates() const noexcept;
```

Checks whether the list's issuingDistributionPoint limits it to CA certificates (onlyContainsCACerts): an end-entity certificate is not covered by it.

## Parameters

None.

## Return value

`true` when the list says so.

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
    println("{}", crl.only_ca_certificates());
}
```

Output:

```text
false
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
