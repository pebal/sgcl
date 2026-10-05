[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_response](README.md)

# sgcl::crypto::x509::ocsp_response::extensions

```cpp
const vector<extension>& extensions() const noexcept;
```

Returns the responseExtensions of the response, in their order ([extension](../x509-extension.md)): the nonce among them when there is one.

## Parameters

None.

## Return value

The extensions.

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
    println("{} {}", response.extensions().size(), nonced.extensions()[0].oid);
}
```

Output:

```text
0 1.3.6.1.5.5.7.48.1.2
```

## See also

- [nonce](nonce.md): the nonce extension's value
- [sgcl::crypto::x509::ocsp_response](README.md)
