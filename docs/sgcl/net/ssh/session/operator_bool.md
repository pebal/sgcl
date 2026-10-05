[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle has a session: `false` for a default-constructed one, `true` for one made by [client::open_session](../client/open_session.md), closed or not.

## Parameters

None.

## Return value

Whether there is a session.

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
    net::ssh::session none;
    net::ssh::session s = c.open_session();
    println("{} {}", (bool)none, (bool)s);
    srv.close();
}
```

Output:

```text
false true
```

## See also

- [(constructor)](session.md)
- [sgcl::net::ssh::session](README.md)
