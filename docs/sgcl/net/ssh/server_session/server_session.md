[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::server_session

```cpp
server_session() noexcept;                          // (1)
server_session(const server_session&) = default;    // (2), implicitly declared
```

1. No session: an operation on it is a contract violation; `operator bool` is `false`. A session is made by the server
   for its handler.
2. The same session: a copy shares it.

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
    srv.handle([](net::ssh::server_session s) {
        net::ssh::server_session none;
        net::ssh::server_session copy = s;
        (void)copy.output().write(to_string((bool)none) + " " + copy.command());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("echo")->out);
    srv.close();
}
```

Output:

```text
false echo
```

## See also

- [server::handle](../server/handle.md)
- [sgcl::net::ssh::server_session](README.md)
