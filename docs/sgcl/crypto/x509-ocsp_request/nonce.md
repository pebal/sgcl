[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::nonce

```cpp
const vector<byte>& nonce() const noexcept;
```

Returns the nonce the request carries (RFC 8954): 16 random bytes when [make](make.md) was asked for one, the bytes of a parsed request's nonce extension. A response that echoes it was made for this request ([ocsp_verify_options](../x509-ocsp_verify_options.md)).

## Parameters

None.

## Return value

The value; empty for a request without a nonce.

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
    auto plain = crypto::x509::ocsp_request::make(leaf, issuer).value();
    auto fresh = crypto::x509::ocsp_request::make(leaf, issuer, {.nonce = true}).value();
    println("{} {}", plain.nonce().size(), fresh.nonce().size());
}
```

Output:

```text
0 16
```

## See also

- [make](make.md): what makes them
- [sgcl::crypto::x509::ocsp_request](README.md)
