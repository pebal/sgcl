[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::certificates

```cpp
const vector<certificate>& certificates() const noexcept;
```

Returns the certificates the responder sent with the response: a delegated responder's own certificate, which its issuer signed for the purpose ([verify](verify.md)); none when the issuer signs its responses itself.

## Parameters

None.

## Return value

The certificates, in order.

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
    println("{}", response.certificates()[0].subject().to_string());
}
```

Output:

```text
CN=SGCL OCSP Responder
```

## See also

- [verify](verify.md): what a delegated responder's certificate must be
- [sgcl::crypto::x509::ocsp_response](README.md)
