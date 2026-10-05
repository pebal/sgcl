[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::with_comment

```cpp
private_key with_comment(const string& comment) const;
```

The same key with another comment: a new handle, the key's bytes copied into its own unmanaged block (zeroed when it goes).

## Parameters

| Parameter | Description |
|---|---|
| `comment` | the new comment |

## Return value

The key with the comment.

## Complexity

Constant (an RSA key's words copied: linear in its size).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::private_key key = net::ssh::private_key::generate().with_comment("deploy@ci");
    println("{}", key.public_key().comment());
}
```

Output:

```text
deploy@ci
```

## See also

- [comment](comment.md)
- [sgcl::net::ssh::private_key](README.md)
