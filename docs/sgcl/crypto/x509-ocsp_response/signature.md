[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::signature

```cpp
const vector<byte>& signature() const noexcept;
```

Returns the response's signature, the bytes of its BIT STRING: over [raw_tbs](raw_tbs.md), by the responder.

## Parameters

None.

## Return value

The signature; empty for a response that is not successful.

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
    println("{}", response.signature().size() > 64);
}
```

Output:

```text
true
```

## See also

- [check_signature_from](check_signature_from.md): the signature checked
- [sgcl::crypto::x509::ocsp_response](README.md)
