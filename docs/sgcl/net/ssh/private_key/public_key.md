[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::public_key

```cpp
ssh::public_key public_key() const noexcept;
```

The public half, with the key's comment: what an authorized_keys file, a .pub file or a known_hosts line holds ([public_key::to_string](../public_key/to_string.md)).

## Parameters

None.

## Return value

The public key.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::load("tests/net/ssh/testdata/ed25519");
    println("{}", key.public_key().to_string());
}
```

Output:

```text
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIO7ky0DPyKt2to+tNXhrGYihNJR5UIxJ5QQifpYnXOMl test-ed25519
```

## See also

- [public_key](../public_key/README.md)
- [sgcl::net::ssh::private_key](README.md)
