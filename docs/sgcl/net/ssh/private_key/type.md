[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::type

```cpp
key_type type() const noexcept;
```

The kind of the key ([key_type](../key_type.md)).

## Parameters

None.

## Return value

The kind.

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
    net::ssh::private_key key = net::ssh::private_key::load("tests/net/ssh/testdata/p256");
    println("{}", key.type() == net::ssh::key_type::ecdsa_p256);
}
```

Output:

```text
true
```

## See also

- [key_type](../key_type.md)
- [sgcl::net::ssh::private_key](README.md)
