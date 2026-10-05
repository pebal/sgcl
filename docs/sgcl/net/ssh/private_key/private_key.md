[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::private_key

```cpp
private_key() noexcept;                       // (1)
private_key(const private_key&) = default;    // (2), implicitly declared
```

1. No key: an operation on it is a contract violation; `operator bool` is `false`.
2. The same key: a copy shares it, its bytes never copied.

## Parameters

None.

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
    net::ssh::private_key none;
    net::ssh::private_key key = net::ssh::private_key::generate();
    net::ssh::private_key copy = key;
    println("{} {}", (bool)none, copy.public_key() == key.public_key());
}
```

Output:

```text
false true
```

## See also

- [generate](generate.md), [load](load.md)
- [sgcl::net::ssh::private_key](README.md)
