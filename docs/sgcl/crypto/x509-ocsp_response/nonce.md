[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::nonce

```cpp
const vector<byte>& nonce() const noexcept;
```

Returns the nonce the response carries (RFC 8954; the bytes of RFC 6960's older form taken too): the request's, echoed by a responder that takes nonces. [verify](verify.md) compares it with the request's when asked.

## Parameters

None.

## Return value

The nonce; empty for a response without one.

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
    auto nonced = crypto::x509::ocsp_response::parse(
        io::read_file(dir + "ocsp_good_nonce.der").value()).value();
    println("{} {}", response.nonce().size(), nonced.nonce().size());
}
```

Output:

```text
0 16
```

## See also

- [ocsp_request::nonce](../x509-ocsp_request/nonce.md): the request's
- [sgcl::crypto::x509::ocsp_response](README.md)
