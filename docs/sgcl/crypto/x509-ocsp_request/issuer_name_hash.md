[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::issuer_name_hash

```cpp
const vector<byte>& issuer_name_hash() const noexcept;
```

Returns the CertID's issuerNameHash: the hash of the DER of the issuer's subject, the certificate's issuer name.

## Parameters

None.

## Return value

The bytes.

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
    println("{}", encoding::hex::encode(request.issuer_name_hash()));
}
```

Output:

```text
f37199f10ed983d99f08272af5ebd3ed4b8d0a8c
```

## See also

- [make](make.md): what makes them
- [sgcl::crypto::x509::ocsp_request](README.md)
