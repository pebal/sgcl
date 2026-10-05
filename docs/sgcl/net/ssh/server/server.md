[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server](README.md)

# sgcl::net::ssh::server::server

```cpp
server() noexcept;                             // (1)
server(const server&) = default;               // (2)
server& operator=(const server&) = default;    // (3)
```

1. A server with no host key, no handler and the default settings.
2. A copy shares the handler and the connections served, and copies the settings: what is set on one after the copy is
   its own. There is no move of its own: a moved-from server is the same server.
3. The same as the copy.

## Parameters

None.

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
    net::ssh::server copy = srv;
    copy.close();  // the same connections
    auto ended = c.wait();
    println("{}", c.is_closed());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [serve](serve.md)
- [sgcl::net::ssh::server](README.md)
