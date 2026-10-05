[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [private_key](README.md)

# sgcl::net::ssh::private_key::comment

```cpp
string comment() const noexcept;
```

The key's comment: what an OpenSSH key file holds after the key (ssh-keygen's `-C`, `user@host` by default); empty for a PEM key and a key made by [generate](generate.md).

## Parameters

None.

## Return value

The comment.

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
    println("{}", net::ssh::private_key::load("tests/net/ssh/testdata/ed25519")->comment());
    println("[{}]", net::ssh::private_key::generate().comment());
}
```

Output:

```text
test-ed25519
[]
```

## See also

- [with_comment](with_comment.md)
- [sgcl::net::ssh::private_key](README.md)
