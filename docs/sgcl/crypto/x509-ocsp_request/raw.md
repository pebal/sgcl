[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::raw

```cpp
slice<const byte> raw() const noexcept;
```

Returns the request's DER: the body of a POST to the responder, of the media type `application/ocsp-request` (RFC
6960 Appendix A.1).

## Parameters

None.

## Return value

A view of the request's bytes, which it keeps alive.

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
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto request = crypto::x509::ocsp_request::make(leaf, issuer).value();
    println("{}", encoding::hex::encode(request.raw()).view().substr(0, 16));
}
```

Output:

```text
30433041303f303d
```

## See also

- [url](url.md): the GET of the same bytes
- [sgcl::crypto::x509::ocsp_request](README.md)
