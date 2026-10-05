[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [authorized_keys](README.md)

# sgcl::net::ssh::authorized_keys::allows

```cpp
bool allows(const string& user, const ssh::public_key& key) const noexcept;
```

Whether `key` lets `user` in, as sshd decides it: a plain key's line, or a certificate signed by the key of a
`cert-authority` line, whose signature verifies, of the user type, valid now, whose principals hold the user — or one
of the line's `principals="…"` names when it has them. A line past its `expiry-time="YYYYMMDD[HHMM[SS]]"` (in the
system's zone) lets no one in. What a [server](../server/README.md)'s `check_public_key` is in one line.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user authenticating |
| `key` | the key or certificate offered |

## Return value

Whether it lets the user in.

## Complexity

Linear in the lines, and a certificate's signature checked.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key ca = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ca.pub"));
    net::ssh::public_key cert = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519-cert.pub"));
    net::ssh::authorized_keys keys = net::ssh::authorized_keys::parse("cert-authority " + ca.to_string());
    println("{} {}", keys.allows("alice", cert), keys.allows("mallory", cert));
}
```

Output:

```text
true false
```

## See also

- [find](find.md)
- [server](../server/README.md)
- [sgcl::net::ssh::authorized_keys](README.md)
