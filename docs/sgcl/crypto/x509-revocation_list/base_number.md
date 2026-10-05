[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::base_number

```cpp
const vector<byte>& base_number() const noexcept;
```

Returns the number of the complete list a delta follows, its deltaCRLIndicator's BaseCRLNumber.

## Parameters

None.

## Return value

The bytes; empty for a list that is not a delta.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    auto delta = crypto::x509::revocation_list::parse(
        io::read_file(dir + "int_delta.crl").value()).value();
    println("{} {}",
        encoding::hex::encode(delta.base_number()), encoding::hex::encode(crl.number()));
}
```

Output:

```text
02 02
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
