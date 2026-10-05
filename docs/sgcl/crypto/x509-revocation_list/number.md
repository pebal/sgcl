[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::number

```cpp
const vector<byte>& number() const noexcept;
```

Returns the list's cRLNumber (RFC 5280 §5.2.3): the bytes of its INTEGER, big-endian, which grows with each list its issuer issues. A delta names the complete list it follows by it ([base_number](base_number.md)).

## Parameters

None.

## Return value

The bytes; empty for a list without one.

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
    println("{}", encoding::hex::encode(crl.number()));
}
```

Output:

```text
02
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
