[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::is_delta

```cpp
bool is_delta() const noexcept;
```

Checks whether the list is a delta CRL (RFC 5280 §5.2.4): one with a deltaCRLIndicator, listing what changed since the complete list of number [base_number](base_number.md). A delta says nothing alone: [status_of](status_of.md) takes it with its complete list.

## Parameters

None.

## Return value

`true` for a delta.

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
    auto delta = crypto::x509::revocation_list::parse(
        io::read_file(dir + "int_delta.crl").value()).value();
    println("{} {}", crl.is_delta(), delta.is_delta());
}
```

Output:

```text
false true
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
