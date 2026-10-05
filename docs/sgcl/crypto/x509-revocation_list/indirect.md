[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::indirect

```cpp
bool indirect() const noexcept;
```

Checks whether the list is indirect (the indirectCRL of its issuingDistributionPoint): it may list certificates of other issuers than its own, each named by its entry's certificateIssuer. [status_of](status_of.md) does not take an indirect list.

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
    println("{}", crl.indirect());
}
```

Output:

```text
false
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
