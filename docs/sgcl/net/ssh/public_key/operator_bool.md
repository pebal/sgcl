[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether there is a key: `false` for an empty one.

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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    println("{} {}", (bool)net::ssh::public_key(), (bool)key);
}
```

Output:

```text
false true
```

## See also

- [(constructor)](public_key.md)
- [sgcl::net::ssh::public_key](README.md)
