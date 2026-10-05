[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::host_key

```cpp
public_key host_key() const noexcept;
```

The server's host key, as the key exchange presented it: a host certificate's blob when the server sent one ([public_key::certificate](../public_key/certificate.md)), a plain key otherwise. The known_hosts line of the server is `host` and this key's [to_string](../public_key/to_string.md).

## Parameters

None.

## Return value

The key, without a comment.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{} {}", c.host_key().type_name(), c.host_key() == srv.host_keys[0].public_key());
    srv.close();
}
```

Output:

```text
ssh-ed25519 true
```

## See also

- [public_key](../public_key/README.md)
- [known_hosts::add](../known_hosts/add.md)
- [sgcl::net::ssh::client](README.md)
