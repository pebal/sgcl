[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::authorized_keys

```cpp
#include "sgcl/net/ssh/authorized_keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class authorized_keys {
    public:
        struct entry;   // a line: its key and its options
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::authorized_keys` is OpenSSH's authorized_keys file: the keys a server lets users in with, a line
`[options] key` each, the options a comma's list of names and `name="value"` pairs ([entry](../authorized_keys-entry/README.md)).
A server's `check_public_key` asks it, in one line, whether a key lets a user in ([allows](allows.md)) as sshd decides
it: a plain key's own line, or a user certificate signed by the key of a `cert-authority` line, valid now, for the
user (or for one of the line's `principals="…"`), within the line's `expiry-time`. What the other options mean
(`command`, `from`, `no-pty`, `permitopen` …) is the program's to apply, from the line [find](find.md) gives: this
module's server runs no command of its own. Go's `ssh.ParseAuthorizedKey`, over a whole file.

A handle of one word over the lines read: copies are the same set, read-only once made, safe from many threads.

## Member types

| Type | Definition |
|---|---|
| [entry](../authorized_keys-entry/README.md) | a line: its key and its options |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](authorized_keys.md) | an empty set |
| [load](load.md) | the lines of a file (static) |
| [parse](parse.md) | the lines of a text (static) |
| [find](find.md) | the line of a key |
| [allows](allows.md) | whether a key lets a user in |
| [size](size.md) | the lines held |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::private_key user_key = net::ssh::private_key::generate();
    net::ssh::authorized_keys keys = net::ssh::authorized_keys::parse(user_key.public_key().to_string());

    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_public_key = [keys](const string& user, const net::ssh::public_key& key) { return keys.allows(user, key); };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("welcome, " + s.user()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.keys = {user_key};
    o.agent = false;
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("")->out);
    srv.close();
}
```

Output:

```text
welcome, ann
```

## See also

- [server](../server/README.md): `check_public_key`
- [public_key](../public_key/README.md), [certificate](../certificate.md)
- [net::ssh](../README.md)
