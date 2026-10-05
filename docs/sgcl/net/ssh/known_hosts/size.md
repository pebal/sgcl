[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::size

```cpp
size_t size() const noexcept;
```

The lines held.

## Parameters

None.

## Return value

The count.

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
    println("{}", net::ssh::known_hosts::parse("a " + key.to_string() + "\nb " + other.to_string()).size());
}
```

Output:

```text
2
```

## See also

- [add](add.md)
- [sgcl::net::ssh::known_hosts](README.md)
