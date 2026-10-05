[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::version

```cpp
int version() const noexcept;
```

Returns the list's version: 2 for a list with a version field (every list with extensions), 1 for one without.

## Parameters

None.

## Return value

1 or 2.

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
    println("{}", crl.version());
}
```

Output:

```text
2
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
