[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle has a key: `false` for a default-constructed one.

## Parameters

None.

## Return value

Whether there is a key.

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
    println("{} {}", (bool)none, (bool)net::ssh::private_key::generate());
}
```

Output:

```text
false true
```

## See also

- [(constructor)](private_key.md)
- [sgcl::net::ssh::private_key](README.md)
