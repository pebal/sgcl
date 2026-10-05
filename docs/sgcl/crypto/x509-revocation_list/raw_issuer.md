[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::raw_issuer

```cpp
slice<const byte> raw_issuer() const noexcept;
```

Returns the DER of the list's issuer name: what a certificate's issuer and its CA's subject are compared with, byte for byte.

## Parameters

None.

## Return value

A view of the list's bytes, which it keeps alive.

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
    println("{}", crl.raw_issuer().size() == issuer.raw_subject().size());
}
```

Output:

```text
true
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
