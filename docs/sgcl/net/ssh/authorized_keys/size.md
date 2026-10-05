[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](README.md)

# sgcl::net::ssh::authorized_keys::size

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
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/p256.pub"));
    println("{}", net::ssh::authorized_keys::parse(key.to_string() + "\nnot a key\n").size());
}
```

Output:

```text
1
```

## See also

- [parse](parse.md)
- [sgcl::net::ssh::authorized_keys](README.md)
