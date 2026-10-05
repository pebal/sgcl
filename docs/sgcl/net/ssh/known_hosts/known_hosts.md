[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::known_hosts

```cpp
known_hosts() noexcept;                       // (1)
known_hosts(const known_hosts&) = default;    // (2), implicitly declared
```

1. An empty set of no file: every host is unknown, and [add](add.md) adds to the set alone.
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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::public_key other = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519_enc.pub"));
    net::ssh::known_hosts hosts;
    net::ssh::known_hosts copy = hosts;
    copy.add("example.com", key);
    println("{} {}", hosts.size(), hosts.check("example.com", key).has_value());
}
```

Output:

```text
1 true
```

## See also

- [load](load.md), [parse](parse.md)
- [sgcl::net::ssh::known_hosts](README.md)
