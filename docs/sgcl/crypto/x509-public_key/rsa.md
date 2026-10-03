[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [public_key](README.md)

# sgcl::crypto::x509::public_key::rsa

```cpp
const crypto::rsa::public_key& rsa() const;
```

Returns the key as an RSA public key, when it is one: `kind()` is `key_kind::rsa`.

## Parameters

None.

## Return value

The [rsa::public_key](../rsa-public_key/README.md).

## Complexity

Constant.

## Exceptions

`logic_error` when the key is of another kind.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    println("{} bits", cert.public_key().rsa().bits());
}
```

Output:

```text
2048 bits
```

## See also

- [rsa::public_key](../rsa-public_key/README.md)
- [sgcl::crypto::x509::public_key](README.md)
