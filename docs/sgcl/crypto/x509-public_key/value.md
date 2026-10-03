[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [public_key](README.md)

# sgcl::crypto::x509::public_key::value

```cpp
const value_type& value() const noexcept;
```

Returns the key as the [variant](../../core/variant/README.md) of the key types, `monostate` for none, for a `visit` that
handles every kind in one place.

## Parameters

None.

## Return value

The variant, its index the `kind()`.

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

    auto bits = visit([](const auto& key) -> size_t {
        if constexpr (requires { key.bits(); }) {
            return key.bits();
        } else {
            return 0;
        }
    }, cert.public_key().value());
    println("{}", bits);
}
```

Output:

```text
2048
```

## See also

- [kind](kind.md)
- [sgcl::crypto::x509::public_key](README.md)
