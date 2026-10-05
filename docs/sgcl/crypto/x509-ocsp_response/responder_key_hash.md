[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::responder_key_hash

```cpp
const vector<byte>& responder_key_hash() const noexcept;
```

Returns the responder of a response that names it by its key (the ResponderID `byKey`): the SHA-1 of the bits of its public key; empty when it is named by name ([responder_name](responder_name.md)).

## Parameters

None.

## Return value

The 20 bytes, or empty.

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
    println("{}", response.responder_key_hash().size());
}
```

Output:

```text
0
```

## See also

- [responder_name](responder_name.md): the other way of naming it
- [sgcl::crypto::x509::ocsp_response](README.md)
