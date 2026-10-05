[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::hash

```cpp
hash_id hash() const noexcept;
```

Returns the hash of the request's CertID: what its issuer's name and key were hashed with ([hash_id](../hash_id.md)).

## Parameters

None.

## Return value

The hash.

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
    auto request = crypto::x509::ocsp_request::make(
        leaf, issuer, {.hash = crypto::hash_id::sha256}).value();
    println("{}", request.hash() == crypto::hash_id::sha256);
}
```

Output:

```text
true
```

## See also

- [make](make.md): what makes them
- [sgcl::crypto::x509::ocsp_request](README.md)
