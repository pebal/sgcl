[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::issuer_key_hash

```cpp
const vector<byte>& issuer_key_hash() const noexcept;
```

Returns the CertID's issuerKeyHash: the hash of the issuer's public key, the bits of its subjectPublicKey.

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
    println("{}", encoding::hex::encode(request.issuer_key_hash()));
}
```

Output:

```text
073f63d17cc7028c844153ff1abc4cdeccf423e0
```

## See also

- [make](make.md): what makes them
- [sgcl::crypto::x509::ocsp_request](README.md)
