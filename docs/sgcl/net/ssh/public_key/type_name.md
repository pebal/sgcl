[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::type_name

```cpp
string type_name() const noexcept;
```

The name of its type as the blob holds it: `ssh-ed25519`, `ecdsa-sha2-nistp256`, `ecdsa-sha2-nistp384`, `ssh-rsa`, or a certificate's, `ssh-ed25519-cert-v01@openssh.com` and the others.

## Parameters

None.

## Return value

The name.

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
    net::ssh::public_key cert = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/host-cert.pub"));
    println("{}\n{}", key.type_name(), cert.type_name());
}
```

Output:

```text
ssh-ed25519
ecdsa-sha2-nistp256-cert-v01@openssh.com
```

## See also

- [type](type.md)
- [sgcl::net::ssh::public_key](README.md)
