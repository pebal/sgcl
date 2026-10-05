[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::raw_tbs

```cpp
slice<const byte> raw_tbs() const noexcept;
```

Returns the ResponseData of the response, the bytes its signature covers; empty for a response that is not successful.

## Parameters

None.

## Return value

A view of the response's bytes, which it keeps alive.

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
    println("{}", response.raw_tbs().size() < response.raw().size());
}
```

Output:

```text
true
```

## See also

- [signature](signature.md): the signature over them
- [sgcl::crypto::x509::ocsp_response](README.md)
