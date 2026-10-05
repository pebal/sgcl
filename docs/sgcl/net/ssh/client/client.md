[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::client

```cpp
client() noexcept;                  // (1)
client(const client&) = default;    // (2), implicitly declared
```

1. No connection: an operation on it is a contract violation; `operator bool` is `false`. A client is made by
   [connect](connect.md).
2. The same connection: a copy shares it.

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
    net::ssh::client none;
    net::ssh::client copy = c;
    println("{} {}", (bool)none, copy.run("ls")->out);
    c.close();
    println("{}", copy.is_closed());
    srv.close();
}
```

Output:

```text
false ran ls
true
```

## See also

- [connect, async_connect](connect.md)
- [sgcl::net::ssh::client](README.md)
