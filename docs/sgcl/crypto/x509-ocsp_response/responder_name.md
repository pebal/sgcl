[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::responder_name

```cpp
const vector<byte>& responder_name() const noexcept;
```

Returns the responder of a response that names it by name (the ResponderID `byName`): the DER of the name, the subject of the certificate that signed the response; empty when it is named by its key ([responder_key_hash](responder_key_hash.md)).

## Parameters

None.

## Return value

The DER of the name, or empty.

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
    auto responder = crypto::x509::certificate::from_pem(
        io::read_text(dir + "responder.pem")).value();
    println("{}", response.responder_name().size() == responder.raw_subject().size());
}
```

Output:

```text
true
```

## See also

- [certificates](certificates.md): the responder's certificate, when it sent it
- [sgcl::crypto::x509::ocsp_response](README.md)
