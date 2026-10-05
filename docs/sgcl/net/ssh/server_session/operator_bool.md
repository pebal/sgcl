[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle has a session: `false` for a default-constructed one, `true` for the handler's, ended or not.

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
#include <string>

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        net::ssh::server_session none;
        (void)s.output().write(to_string((bool)none) + " " + to_string((bool)s));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("make test")->out);
    srv.close();
}
```

Output:

```text
false true
```

## See also

- [(constructor)](server_session.md)
- [sgcl::net::ssh::server_session](README.md)
