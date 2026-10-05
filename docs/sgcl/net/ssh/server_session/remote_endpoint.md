[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::remote_endpoint

```cpp
endpoint remote_endpoint() const noexcept;
```

The client's address and port, as the connection's [remote_endpoint](../../connection/remote_endpoint.md) has it.

## Parameters

None.

## Return value

The endpoint.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write(s.remote_endpoint().address().to_string()); });
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
127.0.0.1
```

## See also

- [net::endpoint](../../endpoint/README.md)
- [sgcl::net::ssh::server_session](README.md)
