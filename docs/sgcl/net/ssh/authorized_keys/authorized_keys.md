[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](README.md)

# sgcl::net::ssh::authorized_keys::authorized_keys

```cpp
authorized_keys() noexcept;                           // (1)
authorized_keys(const authorized_keys&) = default;    // (2), implicitly declared
```

1. An empty set: no key lets anyone in.
2. The same set: a copy shares it.

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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/p256.pub"));
    net::ssh::authorized_keys none;
    println("{} {}", none.size(), none.allows("ann", key));
}
```

Output:

```text
0 false
```

## See also

- [load](load.md), [parse](parse.md)
- [sgcl::net::ssh::authorized_keys](README.md)
