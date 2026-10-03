[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [public_key](README.md)

# sgcl::crypto::x509::public_key::kind

```cpp
key_kind kind() const noexcept;
```

Returns which of the module's key types the key is, or `key_kind::none`.

## Parameters

None.

## Return value

The [key_kind](../x509-key_kind.md).

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
    auto text = io::read_text("tests/net/tls_testdata/rsa.pem");  // a leaf of the tree's test CA
    crypto::x509::certificate cert = crypto::x509::certificate::from_pem(text);

    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{}", cert.public_key().kind() == crypto::x509::key_kind::rsa);
    println("{}", root.public_key().kind() == crypto::x509::key_kind::p256);
}
```

Output:

```text
true
true
```

## See also

- [has_value](has_value.md)
- [sgcl::crypto::x509::public_key](README.md)
