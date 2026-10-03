[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [public_key](../x509-public_key.md)

# sgcl::crypto::x509::public_key::has_value

```cpp
bool has_value() const noexcept;
```

Checks whether the key is one of the module's key types: `kind() != key_kind::none`.

## Parameters

None.

## Return value

`true` when there is a key, `false` for `key_kind::none`.

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

    println("{}", cert.public_key().has_value());
}
```

Output:

```text
true
```

## See also

- [kind](kind.md)
- [sgcl::crypto::x509::public_key](../x509-public_key.md)
