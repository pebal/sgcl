[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [public_key](README.md)

# sgcl::net::ssh::public_key::is_certificate

```cpp
bool is_certificate() const noexcept;
```

Whether the key is an OpenSSH certificate (PROTOCOL.certkeys), whose signature [certificate](certificate.md) checks.

## Parameters

None.

## Return value

Whether it is one.

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
    println("{} {}", key.is_certificate(), cert.is_certificate());
}
```

Output:

```text
false true
```

## See also

- [certificate](certificate.md)
- [sgcl::net::ssh::public_key](README.md)
