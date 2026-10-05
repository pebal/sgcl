[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::with_comment

```cpp
public_key with_comment(const string& comment) const noexcept;
```

The same key with another comment, for its line.

## Parameters

None.

## Return value

The key.

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
    println("{}", key.with_comment("ann@laptop").to_string());
}
```

Output:

```text
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIO7ky0DPyKt2to+tNXhrGYihNJR5UIxJ5QQifpYnXOMl ann@laptop
```

## See also

- [comment](comment.md)
- [sgcl::net::ssh::public_key](README.md)
