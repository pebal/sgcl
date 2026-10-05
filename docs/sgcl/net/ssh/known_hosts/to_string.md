[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [known_hosts](README.md)

# sgcl::net::ssh::known_hosts::to_string

```cpp
string to_string() const noexcept;
```

The lines held, as the file would have them: the comments between them and the lines that could not be read left out, each key with its comment.

## Parameters

None.

## Return value

The lines, each ending in `\n`.

## Complexity

Linear in the lines.

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
    net::ssh::known_hosts hosts = net::ssh::known_hosts::parse("@cert-authority *.example.com " + key.to_string());
    print("{}", hosts.to_string());
}
```

Output:

```text
@cert-authority *.example.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIO7ky0DPyKt2to+tNXhrGYihNJR5UIxJ5QQifpYnXOMl test-ed25519
```

## See also

- [parse](parse.md)
- [sgcl::net::ssh::known_hosts](README.md)
