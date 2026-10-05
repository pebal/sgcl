[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::extensions

```cpp
const vector<extension>& extensions() const noexcept;
```

Returns every extension of the list (its crlExtensions), in their order ([extension](../x509-extension.md)).

## Parameters

None.

## Return value

The extensions.

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
    for (const auto& e : crl.extensions()) {
        println("{} {}", e.oid, e.critical);
    }
}
```

Output:

```text
2.5.29.35 false
2.5.29.20 false
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
