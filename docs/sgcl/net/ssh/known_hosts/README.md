[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::known_hosts

```cpp
#include "sgcl/net/ssh/known_hosts.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class known_hosts;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::known_hosts` is OpenSSH's known_hosts file: the host keys a client trusts, by the names of their hosts. A
line is `[marker] patterns key`: the patterns a comma's list of names with `*` and `?` (`!` in front of one excludes
it), `[host]:port` for a port other than 22, or one name hashed as `ssh-keygen -H` writes it; the marker
`@cert-authority` for a key that signs the hosts' certificates, `@revoked` for a key never to be taken. A set is
[load](load.md)ed from a file (the user's `~/.ssh/known_hosts` by default) or [parse](parse.md)d from text, asked
whether a host's key is known ([check](check.md)), and given new hosts ([add](add.md), appended to its file). A
[client](../client/README.md) checks the server's key against it by default. Go's `knownhosts` package.

A handle of one word: copies are the same set, safe from many threads.

## Rules

- The name a host is known by is `host` on port 22 and `[host]:port` on any other, as OpenSSH writes it; a host's
  name and its address are different names.
- A plain key is known when a line of the host's names holds it; a host certificate when a `@cert-authority` line of
  the host's names holds the authority that signed it, and the certificate is of the host type, valid now, and names
  the host (its name without the port) among its principals.
- A key (or an authority) a `@revoked` line names is refused whatever else the file says.
- Lines that cannot be read (a key of a kind not read here, a broken line) are passed over, as OpenSSH passes them.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](known_hosts.md) | an empty set of no file |
| [load](load.md) | the set of a file (static) |
| [parse](parse.md) | the set of a text (static) |
| [default_path](default_path.md) | the user's file (static) |
| [check](check.md) | whether a key is a host's |
| [add](add.md) | a host and its key added |
| [to_string](to_string.md) | the lines as the file would have them |
| [size](size.md) | the lines held |
| [path](path.md) | the file add appends to |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key key = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519.pub"));
    net::ssh::known_hosts hosts = net::ssh::known_hosts::parse("example.com,*.example.org " + key.to_string());
    println("{}", hosts.check("example.com:22", key).has_value());
    println("{}", hosts.check("www.example.org", key).has_value());
    println("{}", hosts.check("example.net", key).error().code() == net::errc::ssh_host_key_unknown);
}
```

Output:

```text
true
true
true
```

## See also

- [client::options](../client-options.md): `known_hosts`, `host_key_callback`
- [public_key](../public_key/README.md)
- [net::ssh](../README.md)
