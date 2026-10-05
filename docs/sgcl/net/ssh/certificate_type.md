[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::certificate_type

```cpp
#include "sgcl/net/ssh/keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    enum class certificate_type : uint8_t {
        user = 1,
        host = 2,
    };
}
```

What an OpenSSH certificate is for, by its number in the certificate (PROTOCOL.certkeys): a user's key, which a server
takes for the users among its principals, or a host's, which a client takes for the host names among them.

| Value | Description |
|---|---|
| `user` | a user's certificate (ssh-keygen -s without `-h`) |
| `host` | a host's certificate (ssh-keygen -s with `-h`) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key host = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/host-cert.pub"));
    net::ssh::public_key user = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519-cert.pub"));
    println("{} {}", host.certificate()->type == net::ssh::certificate_type::host,
            user.certificate()->type == net::ssh::certificate_type::user);
}
```

Output:

```text
true true
```

## See also

- [certificate](certificate.md)
- [net::ssh](README.md)
