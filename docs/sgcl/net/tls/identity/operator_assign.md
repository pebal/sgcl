[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [identity](README.md)

# sgcl::net::tls::identity::operator=

```cpp
identity& operator=(const identity& other) noexcept;    // (1), implicitly declared
identity& operator=(identity&& other) noexcept;         // (2), implicitly declared
```

Makes this handle one of the identity `other` holds; the two share its chain and its key. The move is the copy:
`other` keeps the identity.

The identity this handle held before is left to the collector when nothing else holds it; its key is zeroed then.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose identity this one takes |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::identity current(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                               crypto::read_secret("tests/net/tls_testdata/ecdsa.key"));
    net::tls::identity renewed(io::read_text("tests/net/tls_testdata/rsa.pem"),
                               crypto::read_secret("tests/net/tls_testdata/rsa.key"));
    current = renewed;
    println("{}", &current.certificates() == &renewed.certificates());
    println("{}", current.certificates()[0].public_key().kind() == crypto::x509::key_kind::rsa);
}
```

Output:

```text
true
true
```

## See also

- [(constructor)](identity.md): an identity made, or a copy
- [sgcl::net::tls::identity](README.md)
