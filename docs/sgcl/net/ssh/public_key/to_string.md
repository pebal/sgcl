[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::to_string

```cpp
string to_string() const noexcept;
```

The key's line: its type's name, the base64 of its blob and its comment when it has one, as a .pub file holds it and an authorized_keys or known_hosts line ends.

## Parameters

None.

## Return value

The line.

## Complexity

Linear in the blob.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    println("{}", key.to_string());
}
```

Output:

```text
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIO7ky0DPyKt2to+tNXhrGYihNJR5UIxJ5QQifpYnXOMl test-ed25519
```

## See also

- [parse](parse.md)
- [sgcl::net::ssh::public_key](README.md)
