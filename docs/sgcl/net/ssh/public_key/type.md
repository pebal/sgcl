[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::type

```cpp
ssh::key_type type() const noexcept;
```

The kind of the key ([key_type](../key_type.md)); a certificate's is the kind of the key it certifies.

## Parameters

None.

## Return value

The kind.

## Complexity

Linear in the blob.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::public_key cert = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/host-cert.pub"));
    println("{} {}", key.type() == net::ssh::key_type::ed25519, cert.type() == net::ssh::key_type::ecdsa_p256);
}
```

Output:

```text
true true
```

## See also

- [type_name](type_name.md)
- [sgcl::net::ssh::public_key](README.md)
